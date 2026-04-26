"""UE5 property category whitelists for response filtering.

Used by Phase A deterministic filters to reduce token usage on heavy
inspection tools. Categories follow UE5 reflection conventions.
"""

from __future__ import annotations

ACTOR_CATEGORIES: dict[str, frozenset[str]] = {
    "transform": frozenset({
        "location", "rotation", "scale",
        "RelativeLocation", "RelativeRotation", "RelativeScale3D",
    }),
    "rendering": frozenset({
        "bHidden", "bHiddenInGame", "bVisible", "CastShadow",
        "Mobility", "CustomDepthStencilValue",
    }),
    "gameplay": frozenset({
        "Tags", "ComponentTags", "bCanBeDamaged",
    }),
    "identity": frozenset({
        "name", "actor_class",
    }),
}

UOBJECT_CATEGORIES: dict[str, frozenset[str]] = {
    "transform": frozenset({
        "RelativeLocation", "RelativeRotation", "RelativeScale3D",
        "AbsoluteLocation", "AbsoluteRotation", "AbsoluteScale",
    }),
    "rendering": frozenset({
        "bHidden", "bVisible", "CastShadow", "Mobility",
        "CustomDepthStencilValue", "bRenderInMainPass",
        "bReceivesDecals",
    }),
    "collision": frozenset({
        "BodyInstance", "CollisionEnabled",
        "CollisionResponseToChannels", "CollisionProfileName",
        "bGenerateOverlapEvents",
    }),
    "gameplay": frozenset({
        "Tags", "ComponentTags", "bCanBeDamaged",
        "bReplicates", "NetUpdateFrequency",
    }),
}

BLUEPRINT_NOISE_NODE_CLASSES: frozenset[str] = frozenset({
    "K2Node_Knot",
})


def filter_by_categories(
    properties: list[dict],
    categories: list[str],
    name_key: str = "name",
    category_map: dict[str, frozenset[str]] | None = None,
) -> list[dict]:
    """Filter property list by category whitelist.

    Returns properties whose name appears in any of the requested categories.
    Unknown categories are ignored. Empty `categories` returns the input as-is.
    """
    if not categories:
        return properties
    if category_map is None:
        category_map = UOBJECT_CATEGORIES
    allowed: set[str] = set()
    for cat in categories:
        allowed.update(category_map.get(cat.strip(), frozenset()))
    if not allowed:
        return properties
    return [p for p in properties if p.get(name_key) in allowed]


def parse_csv(raw: str) -> list[str]:
    """Parse comma-separated string into a clean list (empty entries dropped)."""
    if not raw:
        return []
    return [token.strip() for token in raw.split(",") if token.strip()]
