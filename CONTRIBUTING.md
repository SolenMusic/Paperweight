# Contributing to Paperweight

Paperweight keeps the procedural core small, portable, and unsurprising.

## Repository boundaries

- `include/paperweight/` contains the public C++20 API.
- `src/core/` contains portable C++20 implementation code.
- `app/macos/` contains AppKit code and the smallest practical Objective-C++ bridge.
- `tests/` contains the in-repository test harness and automated tests.
- `examples/materials/` contains canonical `.pmat` examples.

Apple frameworks and Objective-C or Objective-C++ must not appear in the public
headers or portable core. Material generation belongs in the core, never in the
frontend.

## Code conventions

- Use C++20 without compiler extensions.
- Prefer explicit ownership and value types over hidden global state.
- Keep deterministic behaviour independent of platform library implementations.
- Use `PascalCase` for types, `camelCase` for functions and local variables, and
  `snake_case` for filenames.
- Treat warnings in Paperweight targets as defects. Project warning flags are
  private and must not leak into downstream consumers.
- Add focused tests for behaviour, boundary conditions, and failure modes.
- Avoid third-party dependencies unless an issue records a concrete need and
  the maintenance cost is understood.

## Building and testing

From the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

On macOS, the default deployment target is macOS 11 Big Sur. To verify a
universal Intel and Apple Silicon build:

```sh
cmake -S . -B build-universal \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build-universal
ctest --test-dir build-universal --output-on-failure
```

## Scope discipline

Keep each pull request tied to one issue and its acceptance criteria. Features
deferred from the active roadmap milestone should stay deferred; a paperweight
stops being funny when it becomes heavy enough to require a forklift.
