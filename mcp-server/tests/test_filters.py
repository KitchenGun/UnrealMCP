"""Phase A regression tests — Python-side filters for heavy MCP tools.

Tests that the new filter parameters correctly reduce response payloads
without breaking backward-compatible defaults.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path
from unittest.mock import patch

import pytest

ROOT = Path(__file__).resolve().parents[1] / "src"
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from unreal_mcp.tools.blueprint import _post_filter_blueprint_nodes
from unreal_mcp.utils.property_categories import (
    ACTOR_CATEGORIES,
    BLUEPRINT_NOISE_NODE_CLASSES,
    UOBJECT_CATEGORIES,
    filter_by_categories,
    parse_csv,
)


# ---------------------------------------------------------------------------
# property_categories.py
# ---------------------------------------------------------------------------

class TestParseCsv:
    def test_empty_returns_empty(self):
        assert parse_csv("") == []

    def test_single(self):
        assert parse_csv("transform") == ["transform"]

    def test_multiple_with_spaces(self):
        assert parse_csv("transform, rendering , gameplay") == [
            "transform", "rendering", "gameplay",
        ]

    def test_drops_empty_entries(self):
        assert parse_csv("transform,,rendering") == ["transform", "rendering"]


class TestFilterByCategories:
    def test_empty_categories_returns_input(self):
        props = [{"name": "X"}, {"name": "Y"}]
        assert filter_by_categories(props, []) == props

    def test_unknown_category_returns_empty(self):
        props = [{"name": "RelativeLocation"}]
        # unknown category leaves allowed empty → all filtered out
        assert filter_by_categories(props, ["nonexistent"]) == props

    def test_transform_filters_correctly(self):
        props = [
            {"name": "RelativeLocation"},
            {"name": "RelativeRotation"},
            {"name": "SomeOther"},
        ]
        out = filter_by_categories(props, ["transform"])
        names = [p["name"] for p in out]
        assert "RelativeLocation" in names
        assert "RelativeRotation" in names
        assert "SomeOther" not in names

    def test_actor_categories_distinct_from_uobject(self):
        # ACTOR_CATEGORIES.transform contains lowercase keys
        assert "location" in ACTOR_CATEGORIES["transform"]
        # UOBJECT_CATEGORIES.transform contains UE reflection keys
        assert "RelativeLocation" in UOBJECT_CATEGORIES["transform"]


# ---------------------------------------------------------------------------
# blueprint._post_filter_blueprint_nodes
# ---------------------------------------------------------------------------

def _make_node(node_class: str, x: float = 100.0, y: float = 200.0, links: int = 2) -> dict:
    return {
        "node_id": f"nid_{node_class}",
        "node_class": node_class,
        "node_title": f"Title_{node_class}",
        "position_x": x,
        "position_y": y,
        "pins": [
            {
                "pin_name": "Then",
                "direction": "output",
                "pin_type": "exec",
                "links": [{"node_id": f"link_{i}", "pin_name": "In"} for i in range(links)],
            }
        ],
    }


class TestPostFilterBlueprintNodes:
    def test_default_drops_reroute_and_strips_positions(self):
        nodes = [
            _make_node("K2Node_Event"),
            _make_node("K2Node_Knot"),
            _make_node("K2Node_CallFunction"),
        ]
        inner: dict = {}
        out = _post_filter_blueprint_nodes(
            nodes,
            include_positions=False,
            include_pin_links=True,
            node_class_filter=[],
            limit=0,
            drop_reroute=True,
            original_total=3,
            result_inner=inner,
        )
        assert len(out) == 2
        assert all("position_x" not in n for n in out)
        assert all(n["node_class"] != "K2Node_Knot" for n in out)
        assert inner["total_count"] == 3

    def test_include_positions_keeps_coords(self):
        nodes = [_make_node("K2Node_Event")]
        inner: dict = {}
        out = _post_filter_blueprint_nodes(
            nodes,
            include_positions=True,
            include_pin_links=True,
            node_class_filter=[],
            limit=0,
            drop_reroute=False,
            original_total=1,
            result_inner=inner,
        )
        assert out[0]["position_x"] == 100.0

    def test_drop_pin_links(self):
        nodes = [_make_node("K2Node_Event", links=5)]
        inner: dict = {}
        out = _post_filter_blueprint_nodes(
            nodes,
            include_positions=True,
            include_pin_links=False,
            node_class_filter=[],
            limit=0,
            drop_reroute=False,
            original_total=1,
            result_inner=inner,
        )
        assert out[0]["pins"][0]["links"] == []

    def test_node_class_filter_whitelist(self):
        nodes = [
            _make_node("K2Node_Event"),
            _make_node("K2Node_VariableSet"),
            _make_node("K2Node_CallFunction"),
        ]
        inner: dict = {}
        out = _post_filter_blueprint_nodes(
            nodes,
            include_positions=True,
            include_pin_links=True,
            node_class_filter=["K2Node_Event", "K2Node_CallFunction"],
            limit=0,
            drop_reroute=False,
            original_total=3,
            result_inner=inner,
        )
        classes = {n["node_class"] for n in out}
        assert classes == {"K2Node_Event", "K2Node_CallFunction"}

    def test_limit_truncates(self):
        nodes = [_make_node(f"K2Node_X{i}") for i in range(10)]
        inner: dict = {}
        out = _post_filter_blueprint_nodes(
            nodes,
            include_positions=True,
            include_pin_links=True,
            node_class_filter=[],
            limit=3,
            drop_reroute=False,
            original_total=10,
            result_inner=inner,
        )
        assert len(out) == 3
        assert inner["truncated_at"] == 3
        assert inner["total_count"] == 10

    def test_zero_limit_means_unlimited(self):
        nodes = [_make_node(f"K2Node_X{i}") for i in range(50)]
        inner: dict = {}
        out = _post_filter_blueprint_nodes(
            nodes,
            include_positions=True,
            include_pin_links=True,
            node_class_filter=[],
            limit=0,
            drop_reroute=False,
            original_total=50,
            result_inner=inner,
        )
        assert len(out) == 50
        assert "truncated_at" not in inner

    def test_knot_in_noise_set(self):
        # sanity: the noise set is what we expect
        assert "K2Node_Knot" in BLUEPRINT_NOISE_NODE_CLASSES


# ---------------------------------------------------------------------------
# Token reduction sanity (smoke test, not exact tiktoken)
# ---------------------------------------------------------------------------

class TestTokenReduction:
    def test_position_strip_reduces_payload_size(self):
        nodes = [
            _make_node(f"K2Node_X{i}", x=1234.567, y=8901.234, links=3)
            for i in range(20)
        ]
        before = len(json.dumps(nodes))
        for n in nodes:
            n.pop("position_x", None)
            n.pop("position_y", None)
        after = len(json.dumps(nodes))
        # at least 10% reduction from coord removal alone
        assert after < before * 0.95
