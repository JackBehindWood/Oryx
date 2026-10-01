import tomllib



class FakeQuestion:
    def __init__(self, answer):
        self.answer = answer

    def ask(self):
        return self.answer


class FakeQuestionary:
    def __init__(self, choice, text):
        self.choice, self.text_answer = choice, text

    def select(self, message, choices):
        assert self.choice in choices
        return FakeQuestion(self.choice)

    def text(self, message):
        return FakeQuestion(self.text_answer)


def _entry(root):
    return tomllib.loads((root / "forge.toml").read_text())["dependencies"]["lib"]


def test_picker_and_flag_produce_identical_entries(tmp_project, forge, remote, monkeypatch):
    url, first, head = remote
    flagged = forge("deps", "add", "lib", "--git", url)
    assert flagged.exit_code == 0, flagged.output
    expected = _entry(tmp_project)
    forge("deps", "remove", "lib", "--yes")

    from pyforge import interactive
    from pyforge.commands.deps import SOURCE_PROMPTS

    label = next(label for label, (source, _) in SOURCE_PROMPTS.items() if source == "git")
    monkeypatch.setattr(interactive, "is_interactive", lambda: True)
    monkeypatch.setattr(interactive, "questionary_or_none", lambda: FakeQuestionary(label, url))
    picked = forge("deps", "add", "lib")
    assert picked.exit_code == 0, picked.output
    assert _entry(tmp_project) == expected


def test_no_flag_outside_a_tty_keeps_the_flag_error(tmp_project, forge):
    from pyforge.commands.deps import SOURCE_FLAGS_MESSAGE

    result = forge("deps", "add", "lib")
    assert result.exit_code == 1 and SOURCE_FLAGS_MESSAGE in result.output
