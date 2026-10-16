# Test data

- `fonts/PressStart2P-Regular.ttf`, `fonts/OFL.txt`: Press Start 2P (SIL OFL 1.1), from https://github.com/google/fonts/tree/main/ofl/pressstart2p
- `replay/v1_sample.jsonl`: a two-match Tic-Tac-Toe trace exactly as `JsonLinesWriter` writes it (a test fails if the writer's output drifts; regenerate with `OX_WRITE_REPLAY_FIXTURE=1 Tests --test-case="*golden*"`).
- `replay/truncated.jsonl`: `v1_sample.jsonl` cut mid-way through its last decision, as a crashed recorder leaves it.
- `replay/hostile.jsonl`: a BOM, mixed line endings, malformed lines, an unknown kind, out-of-range numbers, nesting, duplicate keys and bad tree parents.
