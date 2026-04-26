"""Phase 7 -- World Builder tools.

Pure-Python orchestration layer. Composes multiple create_actor calls
to place actors in grids, circles, lines, scatter patterns, batches,
or mirrored layouts — no C++ changes required.
"""

import json
import math
import random
from mcp.server.fastmcp import FastMCP as Server

from ..connection import send_command, HEAVY_TIMEOUT
from ..utils.validators import validate_vector3


# ─────────────────────────────────────────────────
# Module-level helpers
# ─────────────────────────────────────────────────

async def _spawn_one(
    actor_class: str,
    name: str,
    location: list[float],
    rotation: list[float],
    scale: list[float],
) -> dict:
    """단일 create_actor 명령을 전송하고 응답 dict를 반환한다."""
    command = {
        "type": "create_actor",
        "params": {
            "actor_class": actor_class.strip(),
            "name": name,
            "location": location,
            "rotation": rotation,
            "scale": scale,
        },
    }
    return await send_command(command)


def _build_summary(
    results: list[dict],
    total_requested: int,
    tool_name: str,
) -> str:
    """결과 목록에서 성공/실패를 집계해 JSON 요약 문자열을 반환한다."""
    succeeded = [r for r in results if r.get("success")]
    failed = [r for r in results if not r.get("success")]
    actor_names = [
        r.get("result", {}).get("name", "")
        for r in succeeded
        if r.get("result")
    ]
    return json.dumps(
        {
            "success": len(failed) == 0,
            "tool": tool_name,
            "requested": total_requested,
            "spawned": len(succeeded),
            "failed": len(failed),
            "actor_names": actor_names,
            "errors": [r.get("error") for r in failed],
        },
        indent=2,
        ensure_ascii=False,
    )


# ─────────────────────────────────────────────────
# Registration
# ─────────────────────────────────────────────────

def register_world_builder_tools(server: Server) -> None:
    """Phase 7 World Builder 도구들을 MCP 서버에 등록한다."""

    @server.tool("spawn_actor_grid")
    async def spawn_actor_grid(
        actor_class: str,
        rows: int = 3,
        cols: int = 3,
        spacing_x: float = 200.0,
        spacing_y: float = 200.0,
        origin: tuple[float, float, float] = (0.0, 0.0, 0.0),
        rotation: tuple[float, float, float] = (0.0, 0.0, 0.0),
        scale: tuple[float, float, float] = (1.0, 1.0, 1.0),
    ) -> str:
        """[WorldBuilder] N×M 그리드로 동일한 액터를 반복 배치한다.

        Args:
            actor_class: 생성할 UE5 액터 클래스 이름 (예: "PointLight", "StaticMeshActor")
            rows: 행 수 (Y축 방향)
            cols: 열 수 (X축 방향)
            spacing_x: 열 간격 (UE 단위, X축)
            spacing_y: 행 간격 (UE 단위, Y축)
            origin: 그리드 시작 위치 [X, Y, Z]
            rotation: 각 액터의 회전값 [Pitch, Yaw, Roll]
            scale: 각 액터의 스케일 [X, Y, Z]
        """
        if rows < 1 or cols < 1:
            return json.dumps({
                "success": False,
                "error": {"code": "INVALID_PARAMS", "message": "rows와 cols는 1 이상이어야 합니다."},
            }, indent=2, ensure_ascii=False)

        ox, oy, oz = validate_vector3(origin, "origin")
        rot = validate_vector3(rotation, "rotation")
        sc = validate_vector3(scale, "scale")

        results = []
        for i in range(rows):
            for j in range(cols):
                loc = [ox + j * spacing_x, oy + i * spacing_y, oz]
                try:
                    r = await _spawn_one(actor_class, "", loc, rot, sc)
                except Exception as exc:
                    r = {"success": False, "error": {"code": "SPAWN_ERROR", "message": str(exc)}}
                results.append(r)

        return _build_summary(results, rows * cols, "spawn_actor_grid")

    @server.tool("spawn_actor_circle")
    async def spawn_actor_circle(
        actor_class: str,
        count: int = 8,
        radius: float = 500.0,
        origin: tuple[float, float, float] = (0.0, 0.0, 0.0),
        face_center: bool = False,
        scale: tuple[float, float, float] = (1.0, 1.0, 1.0),
    ) -> str:
        """[WorldBuilder] 원형으로 액터를 균등 배치한다.

        Args:
            actor_class: 생성할 UE5 액터 클래스 이름
            count: 배치할 액터 수
            radius: 원 반지름 (UE 단위)
            origin: 원 중심 위치 [X, Y, Z]
            face_center: True이면 각 액터가 원 중심을 향하도록 Yaw 회전
            scale: 각 액터의 스케일 [X, Y, Z]
        """
        if count < 1:
            return json.dumps({
                "success": False,
                "error": {"code": "INVALID_PARAMS", "message": "count는 1 이상이어야 합니다."},
            }, indent=2, ensure_ascii=False)

        ox, oy, oz = validate_vector3(origin, "origin")
        sc = validate_vector3(scale, "scale")

        results = []
        for i in range(count):
            theta = 2 * math.pi * i / count
            loc = [ox + radius * math.cos(theta), oy + radius * math.sin(theta), oz]
            if face_center:
                yaw = math.degrees(math.atan2(-math.sin(theta), -math.cos(theta)))
                rot = [0.0, yaw, 0.0]
            else:
                rot = [0.0, 0.0, 0.0]
            try:
                r = await _spawn_one(actor_class, "", loc, rot, sc)
            except Exception as exc:
                r = {"success": False, "error": {"code": "SPAWN_ERROR", "message": str(exc)}}
            results.append(r)

        return _build_summary(results, count, "spawn_actor_circle")

    @server.tool("spawn_actor_scatter")
    async def spawn_actor_scatter(
        actor_class: str,
        count: int = 10,
        area_width: float = 1000.0,
        area_depth: float = 1000.0,
        origin: tuple[float, float, float] = (0.0, 0.0, 0.0),
        seed: int = 42,
        height_variance: float = 0.0,
    ) -> str:
        """[WorldBuilder] 지정 영역 내에 액터를 결정론적으로 랜덤 산포한다.

        같은 seed 값을 사용하면 항상 동일한 배치 결과를 보장한다.

        Args:
            actor_class: 생성할 UE5 액터 클래스 이름
            count: 배치할 액터 수
            area_width: 산포 영역 너비 (X축, UE 단위)
            area_depth: 산포 영역 깊이 (Y축, UE 단위)
            origin: 영역 중심 위치 [X, Y, Z]
            seed: 랜덤 시드 (재현 가능한 결과를 위해 사용)
            height_variance: Z축 높이 무작위 변동 범위 (±값)
        """
        if count < 1:
            return json.dumps({
                "success": False,
                "error": {"code": "INVALID_PARAMS", "message": "count는 1 이상이어야 합니다."},
            }, indent=2, ensure_ascii=False)

        ox, oy, oz = validate_vector3(origin, "origin")
        rng = random.Random(seed)

        results = []
        for _ in range(count):
            x = ox + rng.uniform(-area_width / 2, area_width / 2)
            y = oy + rng.uniform(-area_depth / 2, area_depth / 2)
            z = oz + rng.uniform(-height_variance, height_variance)
            try:
                r = await _spawn_one(actor_class, "", [x, y, z], [0.0, 0.0, 0.0], [1.0, 1.0, 1.0])
            except Exception as exc:
                r = {"success": False, "error": {"code": "SPAWN_ERROR", "message": str(exc)}}
            results.append(r)

        return _build_summary(results, count, "spawn_actor_scatter")

    @server.tool("spawn_actor_line")
    async def spawn_actor_line(
        actor_class: str,
        count: int = 5,
        start: tuple[float, float, float] = (0.0, 0.0, 0.0),
        end: tuple[float, float, float] = (1000.0, 0.0, 0.0),
        scale: tuple[float, float, float] = (1.0, 1.0, 1.0),
    ) -> str:
        """[WorldBuilder] 두 점 사이에 액터를 선형으로 균등 배치한다.

        Args:
            actor_class: 생성할 UE5 액터 클래스 이름
            count: 배치할 액터 수
            start: 시작 위치 [X, Y, Z]
            end: 끝 위치 [X, Y, Z]
            scale: 각 액터의 스케일 [X, Y, Z]
        """
        if count < 1:
            return json.dumps({
                "success": False,
                "error": {"code": "INVALID_PARAMS", "message": "count는 1 이상이어야 합니다."},
            }, indent=2, ensure_ascii=False)

        s = validate_vector3(start, "start")
        e = validate_vector3(end, "end")
        sc = validate_vector3(scale, "scale")

        results = []
        for i in range(count):
            t = 0.5 if count == 1 else i / (count - 1)
            loc = [s[k] + t * (e[k] - s[k]) for k in range(3)]
            try:
                r = await _spawn_one(actor_class, "", loc, [0.0, 0.0, 0.0], sc)
            except Exception as exc:
                r = {"success": False, "error": {"code": "SPAWN_ERROR", "message": str(exc)}}
            results.append(r)

        return _build_summary(results, count, "spawn_actor_line")

    @server.tool("spawn_actor_batch")
    async def spawn_actor_batch(
        actors: list[dict],
    ) -> str:
        """[WorldBuilder] 다수의 액터 명세를 한 번에 일괄 생성한다.

        각 명세(dict)에는 다음 필드를 포함할 수 있다:
        - class (필수): UE5 액터 클래스 이름
        - name (선택): 액터 이름
        - location (선택): [X, Y, Z] (기본값: [0, 0, 0])
        - rotation (선택): [Pitch, Yaw, Roll] (기본값: [0, 0, 0])
        - scale (선택): [X, Y, Z] (기본값: [1, 1, 1])

        예시:
        [
          {"class": "PointLight", "name": "Light_A", "location": [0, 0, 300]},
          {"class": "StaticMeshActor", "location": [500, 0, 0], "scale": [2, 2, 2]}
        ]

        Args:
            actors: 생성할 액터 명세 목록
        """
        if not actors:
            return json.dumps({
                "success": False,
                "error": {"code": "INVALID_PARAMS", "message": "actors 목록이 비어있습니다."},
            }, indent=2, ensure_ascii=False)

        results = []
        for spec in actors:
            actor_class = spec.get("class", "").strip()
            if not actor_class:
                results.append({
                    "success": False,
                    "error": {"code": "INVALID_SPEC", "message": "'class' 필드가 없거나 비어있습니다."},
                })
                continue

            try:
                loc = validate_vector3(spec.get("location", [0.0, 0.0, 0.0]), "location")
                rot = validate_vector3(spec.get("rotation", [0.0, 0.0, 0.0]), "rotation")
                sc = validate_vector3(spec.get("scale", [1.0, 1.0, 1.0]), "scale")
            except ValueError as exc:
                results.append({
                    "success": False,
                    "error": {"code": "INVALID_PARAMS", "message": str(exc)},
                })
                continue

            name = spec.get("name", "")
            command = {
                "type": "create_actor",
                "params": {
                    "actor_class": actor_class,
                    "name": name,
                    "location": loc,
                    "rotation": rot,
                    "scale": sc,
                },
            }
            try:
                r = await send_command(command, timeout=HEAVY_TIMEOUT)
            except Exception as exc:
                r = {"success": False, "error": {"code": "SPAWN_ERROR", "message": str(exc)}}
            results.append(r)

        return _build_summary(results, len(actors), "spawn_actor_batch")

    @server.tool("mirror_actors")
    async def mirror_actors(
        actor_names: list[str],
        axis: str = "X",
        origin: tuple[float, float, float] = (0.0, 0.0, 0.0),
    ) -> str:
        """[WorldBuilder] 액터 목록을 지정한 축 기준으로 대칭 복제한다.

        원본 액터의 위치를 origin 기준으로 반사한 위치에 새 액터를 생성한다.
        복제된 액터 이름은 '{원본이름}_Mirror' 규칙을 따른다.

        Args:
            actor_names: 대칭 복제할 원본 액터 이름 목록
            axis: 대칭 축 ("X", "Y", "Z" 중 하나)
            origin: 대칭 기준점 [X, Y, Z]
        """
        if not actor_names:
            return json.dumps({
                "success": False,
                "error": {"code": "INVALID_PARAMS", "message": "actor_names 목록이 비어있습니다."},
            }, indent=2, ensure_ascii=False)

        axis = axis.upper()
        if axis not in ("X", "Y", "Z"):
            return json.dumps({
                "success": False,
                "error": {"code": "INVALID_PARAMS", "message": "axis는 'X', 'Y', 'Z' 중 하나여야 합니다."},
            }, indent=2, ensure_ascii=False)

        ox, oy, oz = validate_vector3(origin, "origin")

        results = []
        for name in actor_names:
            name = name.strip()

            # 원본 액터 속성 조회
            try:
                props_resp = await send_command({
                    "type": "get_actor_properties",
                    "params": {"name": name},
                })
            except Exception as exc:
                results.append({
                    "success": False,
                    "error": {"code": "QUERY_ERROR", "message": str(exc)},
                })
                continue

            if not props_resp.get("success"):
                results.append({
                    "success": False,
                    "error": props_resp.get("error"),
                })
                continue

            actor_data = props_resp.get("result", {})

            # location은 {"x": ..., "y": ..., "z": ...} dict 형식으로 반환됨
            loc_dict = actor_data.get("location", {})
            rot_dict = actor_data.get("rotation", {})
            sc_dict = actor_data.get("scale", {})
            actor_class = actor_data.get("actor_class", "Actor")

            loc = [
                float(loc_dict.get("x", 0.0)),
                float(loc_dict.get("y", 0.0)),
                float(loc_dict.get("z", 0.0)),
            ]
            rot = [
                float(rot_dict.get("pitch", 0.0)),
                float(rot_dict.get("yaw", 0.0)),
                float(rot_dict.get("roll", 0.0)),
            ]
            sc = [
                float(sc_dict.get("x", 1.0)),
                float(sc_dict.get("y", 1.0)),
                float(sc_dict.get("z", 1.0)),
            ]

            # 대칭 위치 계산
            new_loc = list(loc)
            if axis == "X":
                new_loc[0] = 2 * ox - loc[0]
            elif axis == "Y":
                new_loc[1] = 2 * oy - loc[1]
            else:
                new_loc[2] = 2 * oz - loc[2]

            mirror_name = f"{name}_Mirror"
            try:
                r = await _spawn_one(actor_class, mirror_name, new_loc, rot, sc)
            except Exception as exc:
                r = {"success": False, "error": {"code": "SPAWN_ERROR", "message": str(exc)}}

            results.append(r)

        return _build_summary(results, len(actor_names), "mirror_actors")
