"""Parameter validation utilities."""

from typing import Any


def validate_vector3(value: Any, name: str) -> list[float]:
    """Validate and convert 3D vector parameter in (X, Y, Z) format to list.

    Args:
        value: Value to validate (tuple or list).
        name: Parameter name to display in error messages.

    Returns:
        Float list in [X, Y, Z] format.

    Raises:
        ValueError: If format is incorrect.
    """
    try:
        result = [float(v) for v in value]
    except (TypeError, ValueError) as e:
        raise ValueError(
            f"Parameter '{name}' must be 3 numbers in (X, Y, Z) format. "
            f"Input: {value!r}"
        ) from e

    if len(result) != 3:
        raise ValueError(
            f"Parameter '{name}' requires exactly 3 values (X, Y, Z). "
            f"Input length: {len(result)}"
        )
    return result


def validate_actor_name(name: str) -> str:
    """Validate actor name parameter.

    Args:
        name: Actor name.

    Returns:
        Actor name with whitespace removed.

    Raises:
        ValueError: If name is empty.
    """
    name = name.strip()
    if not name:
        raise ValueError("Actor name cannot be empty.")
    return name
