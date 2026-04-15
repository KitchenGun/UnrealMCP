# no-print-debug

## Occurrence Date
2026-04-06

## Failure Scenario
Left `print("debug ...")` code in deployment -> Production log pollution

## Rule
- Prohibit debug-related prints: `print("debug`, `print("test`, `print("TODO`, etc
- Use `logging` module if logging is needed

## Auto-Correction
In `--fix` mode, convert offending line to `# REMOVED:` comment

## Related Files
- `rules/no-print-debug.sh` -- Automated checking script
