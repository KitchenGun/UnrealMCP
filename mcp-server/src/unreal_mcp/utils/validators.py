"""파라미터 검증 유틸리티."""

from typing import Any


def validate_vector3(value: Any, name: str) -> list[float]:
    """(X, Y, Z) 형식의 3D 벡터 파라미터를 검증하고 리스트로 변환한다.

    Args:
        value: 검증할 값 (tuple 또는 list).
        name: 에러 메시지에 표시할 파라미터 이름.

    Returns:
        [X, Y, Z] 형식의 float 리스트.

    Raises:
        ValueError: 형식이 올바르지 않은 경우.
    """
    try:
        result = [float(v) for v in value]
    except (TypeError, ValueError) as e:
        raise ValueError(
            f"파라미터 '{name}'은 (X, Y, Z) 형식의 숫자 3개여야 합니다. "
            f"입력값: {value!r}"
        ) from e

    if len(result) != 3:
        raise ValueError(
            f"파라미터 '{name}'은 정확히 3개의 값이 필요합니다 (X, Y, Z). "
            f"입력값 길이: {len(result)}"
        )
    return result


def validate_actor_name(name: str) -> str:
    """액터 이름 파라미터를 검증한다.

    Args:
        name: 액터 이름.

    Returns:
        공백이 제거된 액터 이름.

    Raises:
        ValueError: 이름이 비어있는 경우.
    """
    name = name.strip()
    if not name:
        raise ValueError("액터 이름은 비어있을 수 없습니다.")
    return name
