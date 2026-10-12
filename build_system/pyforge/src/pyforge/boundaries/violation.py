from dataclasses import dataclass


@dataclass(frozen=True)
class Violation:
    subject: str
    message: str
    path: str = ""
    line: int = 0
    rule: str = ""
    spec: str = ""

    @property
    def location(self) -> str:
        """`path:line` for editors and CI annotations; the subject alone when it isn't a source line."""
        return f"{self.path}:{self.line}" if self.path and self.line else (self.path or self.subject)

    def __str__(self) -> str:
        return f"{self.subject}: {self.message}"
