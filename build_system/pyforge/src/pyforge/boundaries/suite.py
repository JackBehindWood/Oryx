from collections.abc import Iterator

from ..config.schema import SchemaError
from .spec import BoundarySpec


class BoundarySuite:
    """Boundaries for a project that mixes languages: one BoundarySpec per language or per tree, checked together.

        suite = BoundarySuite()
        native = suite.new("cpp", root="src", namespace="app")
        tooling = suite.new("python", root="tools")
    """

    def __init__(self, *specs: BoundarySpec) -> None:
        self.specs: dict[str, BoundarySpec] = {}
        for spec in specs:
            self.add(spec)

    def add(self, spec: BoundarySpec) -> BoundarySpec:
        if spec.name in self.specs:
            raise SchemaError(f"boundaries: two specs are named '{spec.name}'; give one a distinct name=")
        self.specs[spec.name] = spec
        return spec

    def new(self, scan: str, *, name: str = "", root: str = ".", base: str = "", namespace: str = "", message: str = "") -> BoundarySpec:
        """Create a spec for the language `scan` and add it; `name` defaults to the language, so give one when a language appears twice."""
        return self.add(BoundarySpec(root, base, scan, name=name, namespace=namespace, message=message))

    def __iter__(self) -> Iterator[BoundarySpec]:
        return iter(self.specs.values())

    def __len__(self) -> int:
        return len(self.specs)


def specs_of(value: "BoundarySpec | BoundarySuite | list[BoundarySpec] | tuple[BoundarySpec, ...] | None") -> list[BoundarySpec]:
    """What a plugin's `forge_boundaries` may return, normalised to a list of specs."""
    if value is None:
        return []
    if isinstance(value, BoundarySpec):
        return [value]
    if isinstance(value, BoundarySuite):
        return list(value.specs.values())
    return list(value)
