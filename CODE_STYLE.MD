# Code Style & Naming Manifesto

This document defines the formatting, naming, and general design conventions that govern the project’s source code. Adhering to these guidelines ensures a cohesive, predictable codebase that is easier to read, review, and maintain.

---
## 1  Formatting

* **Single source of truth** – all mechanical formatting decisions are delegated to **clang-format**. The project is in active development, so the `.clang-format` file may evolve until the first stable release.
* **No manual overrides** – do not hand‑tweak code to fight the formatter. Instead, propose a change to the style file if a rule proves counter‑productive.

---
## 2  Namespaces & File Layout

* Every public symbol **must live inside a namespace**. Free‑floating types or functions are forbidden.
* Organize code so that each library or module forms a coherent domain: if you introduce a helper for generating timeouts, create a small `timeout` library instead of burying the function in `libA`.
* Keep header / source pairs small and focused; one logical unit per file whenever practical.

---
## 3  Naming Conventions

| Entity | Format | Example |
|--------|--------|---------|
| **Classes / Structs** | `snake_case` + `_t` suffix | `append_only_file_t` |
| **Enums** | `snake_case` + `_k` suffix | `file_error_k` |
| **Enum values** | `k` prefix + `UpperCamelCase` | `kWriteFailed` |
| **Functions / Methods** | `snake_case` | `write_segment()` |
| **Variables** | `snake_case` | `buffer_size` |

Additional rules:

* **Classes vs Structs** – use `struct` only for passive data carriers whose members are all public. If any behavior or invariants are needed, promote it to a `class`, make data private, and expose a minimal interface.
* **Method names are verbs** – e.g. `segments()`, not `get_segments_array()`. Methods returning std::optional<> or std::expected<> should be prefix with `maybe_` e.g. `maybe_create_consensus_module`.
* **Return values matter** – mark any function that returns a value with `[[nodiscard]]` unless discarding the result is genuinely harmless.

---
## 4  Error Handling

* **Prefer `std::expected` over exceptions** for all recoverable errors, especially on deep call paths.
* Each module exports an **error‑code enum** (suffixed `_k`) and an accompanying **error struct** that carries both the code and a human‑readable message.

```cpp
using file_result = std::expected<my_file_t, my_error_t>;
```

* Document all possible error codes so the caller can prepare for them.

---
## 5  Design Philosophy

The codebase does not subscribe rigidly to any single paradigm (OOP, FP, generic programming). We use classes, templates, CRTP, type‑erasure, and map‑reduce techniques pragmatically, choosing the tool that yields the clearest solution.

> **Pragmatism over dogma.** Use the style guide to maintain consistency, but do not let it stifle well‑reasoned deviations when they demonstrably improve clarity or performance.

---
## 6  Contributing Checklist

1. **Run `clang‑format`** before committing.
2. **Follow namespace and file‑placement rules** – no orphan symbols.
3. **Name things correctly** per the table above.
4. **Return errors via `std::expected`**; extend the module’s error enum if necessary.
5. **Add unit tests** that cover both success and failure cases.
6. **Document public APIs** with Doxygen‑style comments.

Keeping these conventions front‑of‑mind lets us move fast without sacrificing code quality. Thank you for helping maintain a clean, coherent codebase!

