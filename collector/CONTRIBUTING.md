# Contributing

Contributions should preserve the single-root file layout and C17 style. Keep modules focused, use explicit ownership rules, and maintain clean error handling for allocation, I/O, malformed telemetry, and shutdown paths.

Before submitting changes:

```sh
cmake -S . -B build -DTP_ENABLE_TESTS=ON -DTP_ENABLE_WARNINGS_AS_ERRORS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Coding expectations:

- Use descriptive names and small public interfaces.
- Avoid hidden heap allocation in steady-state paths.
- Add tests for boundary conditions, malformed input, and recovery behavior.
- Keep log messages actionable and include subsystem context.
- Preserve storage compatibility or document version changes in `TELEMETRY_FORMAT.md`.
- Update documentation when CLI, configuration, telemetry schema, or operational behavior changes.

Commits should be cohesive and explain the behavior change. Pull requests should include test results, risk notes, and any compatibility impact.
