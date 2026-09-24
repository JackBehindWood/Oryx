from dataclasses import dataclass, field

import pluggy

from ..plugins import get_plugin_manager
from ..project import Project
from .load import LOCAL_CONFIG_NAME, load_config, load_local, local_config_file, parse_config, save_local, validate_local
from .schema import (
    DEPENDENCY_SOURCES,
    DOCS_TOOLS,
    EDITORS,
    GENERATORS,
    Choices,
    BuildTable,
    Debugger,
    Dependency,
    DependencyKind,
    DocsTable,
    EditorTable,
    FetchMode,
    ForgeConfig,
    LocalConfig,
    OptionSpec,
    PluginsTable,
    PremakeTable,
    Profile,
    ProjectTable,
    SchemaError,
    Suite,
    Target,
    TestsTable,
    leaf_type,
    parse_scalar,
)

__all__ = [
    "DEPENDENCY_SOURCES",
    "DOCS_TOOLS",
    "EDITORS",
    "GENERATORS",
    "Choices",
    "LOCAL_CONFIG_NAME",
    "BuildTable",
    "Debugger",
    "Dependency",
    "DependencyKind",
    "DocsTable",
    "EditorTable",
    "FetchMode",
    "ForgeConfig",
    "LocalConfig",
    "OptionSpec",
    "PluginsTable",
    "PremakeTable",
    "Profile",
    "ProjectTable",
    "RunContext",
    "SchemaError",
    "Suite",
    "Target",
    "TestsTable",
    "leaf_type",
    "load_config",
    "load_local",
    "local_config_file",
    "parse_config",
    "parse_scalar",
    "save_local",
    "validate_local",
]


@dataclass
class RunContext:
    """Global CLI runtime state, shared as the Typer context object."""
    project: Project
    config: ForgeConfig
    profile: Profile
    options: dict[str, bool]
    local: LocalConfig = field(default_factory=LocalConfig)
    defines: list[str] = field(default_factory=list)
    verbose: bool = False
    dry_run: bool = False
    interactive: bool = False
    pm: pluggy.PluginManager = field(default_factory=get_plugin_manager)

    @property
    def jobs(self) -> int:
        return self.config.build.jobs if self.local.build.jobs is None else self.local.build.jobs

    @property
    def fetch(self) -> FetchMode:
        return self.local.build.fetch or self.config.build.fetch
