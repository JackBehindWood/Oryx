import enum
import inspect
import sys
import types
import typing

import typer

from build_system import registry

BACK = "← Back"
QUIT = "Quit"


def is_interactive() -> bool:
    """Whether an interactive menu can be shown (a real terminal, not CI/pipes)."""
    return sys.stdin.isatty() and sys.stdout.isatty()


def run_menu(ctx: typer.Context) -> None:
    """Present the arrow-key command menu, dispatching actions until the user quits.

    Each completed action returns to the top-level menu rather than exiting,
    so e.g. Build > Compile followed by Test > Run works in one `uv run forge`
    session. A failing action still exits immediately, since the underlying
    command raises typer.Exit(code=1), which unwinds out of this loop.
    """
    import questionary

    while True:
        group_choices = registry.groups_in_order() + [QUIT]
        group_name = questionary.select("What would you like to do?", choices=group_choices).ask()

        if group_name is None or group_name == QUIT:
            return

        entries = registry.entries_for_group(group_name)
        action_choices = [entry.label for entry in entries] + [BACK]
        action_label = questionary.select(f"{group_name} — choose an action:", choices=action_choices).ask()

        if action_label is None or action_label == BACK:
            continue

        entry = next(e for e in entries if e.label == action_label)
        _dispatch(ctx, entry.func)


def _unwrap_optional(annotation):
    """Strip `X | None` / `Optional[X]` down to `X`, so Literal-detection
    works on a param typed either way."""
    origin = typing.get_origin(annotation)
    if origin is typing.Union or origin is types.UnionType:
        args = [a for a in typing.get_args(annotation) if a is not type(None)]
        if len(args) == 1:
            return args[0]
    return annotation


def _is_literal(annotation) -> bool:
    return typing.get_origin(annotation) is typing.Literal


def _dispatch(ctx: typer.Context, func) -> None:
    kwargs = _prompt_for_extra_params(func)
    if kwargs is None:
        return
    ctx.invoke(func, ctx, **kwargs)


def _prompt_for_extra_params(func) -> dict | None:
    """Prompt for every parameter beyond `ctx`, inferring a widget from its
    type/default:
      - bool                              -> questionary.confirm
      - Literal[...] or an Enum (or either `| None`) -> questionary.select
      - anything else                     -> questionary.text

    Uses the typer.Option's own `help=` text as the prompt (falls back to
    the bare parameter name), so any command that adds a well-documented
    Option gets a sensible interactive prompt for free — no per-command
    special-casing needed here. Returns None if the user cancels any prompt
    (Ctrl-C/Esc), matching how a None menu selection aborts today.
    """
    import questionary

    kwargs = {}
    for name, param in inspect.signature(func).parameters.items():
        if name == "ctx":
            continue

        default = param.default
        is_parameter = isinstance(default, typer.models.ParameterInfo)
        typer_default = default.default if is_parameter else default
        help_text = (default.help if is_parameter else None) or name
        annotation = _unwrap_optional(param.annotation)

        if typing.get_origin(annotation) is list:
            answer = questionary.text(f"{help_text} (space-separated)", default="").ask()
            answer = None if answer is None else answer.split()
        elif annotation is bool:
            answer = questionary.confirm(help_text, default=bool(typer_default)).ask()
        elif isinstance(annotation, type) and issubclass(annotation, enum.Enum):
            choices = [str(member.value) for member in annotation]
            default_choice = str(typer_default.value) if isinstance(typer_default, enum.Enum) else None
            answer = questionary.select(help_text, choices=choices, default=default_choice).ask()
            answer = None if answer is None else annotation(answer)
        elif _is_literal(annotation):
            choices = [str(choice) for choice in typing.get_args(annotation)]
            default_choice = typer_default if typer_default in choices else None
            answer = questionary.select(help_text, choices=choices, default=default_choice).ask()
        else:
            answer = questionary.text(help_text, default=str(typer_default) if typer_default is not None else "").ask()

        if answer is None:
            return None
        kwargs[name] = answer
    return kwargs
