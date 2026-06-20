  ## Roadmap toward v0.0.3 and beyond

### Features (new capability)

  - [ ] `arena_read_file` helper — the README's own "Temporary File Processing" use case hand-rolls fopen/fseek/ftell/fread into an arena; that's common enough to be a real function instead of copy-pasted boilerplate each time.
  - [ ] str8 operations — equality/comparison, slicing, trim, find, split. Right now str8 can be pushed/copied/formatted but not actually manipulated; the data-processing/parsing work this is meant for will want these soon.
  - [ ] POSIX backend for the OS layer — `os_mem_reserve/commit/decommit/release/pagesize` are Windows-only by design for now. The arena logic itself doesn't change, just those five shim functions, whenever CFD/HPC work lands you on  Linux.
  - [ ] add `os_file_map` / `os_file_unmap` using `CreateFileMapping`/`MapViewOfFile` for direct mem-mapping of OS files. (for WIN32 for now)

### Quality of life (easier to use, same capability)
  - [ ] Scratch/temp-arena helper (`ArenaTemp` + `arena_temp_begin`/`arena_temp_end`) — wraps the manual `u64 mark = arena.pos; ... arena_pop_to(&arena, mark);` pattern you already do by hand everywhere.
  - [ ] `CHANGELOG.md` — now that real tags exist, worth more than `gh release --generate-notes` alone once there's more than one release to compare across.
  - [ ] GitHub Actions CI — build + `ctest` on push/PR. Cheap now that there's a real test suite; catches regressions before they reach a tag.

  ### Hardening / process
  - [ ] Decide an error-handling convention before adding more I/O-shaped APIs. Arenas now hard-abort on OOM — a fine, deliberate choice for memory. `arena_read_file` and friends will hit a *different* failure shape (missing file, permission denied) that isn't obviously "abort-worthy" the same way. Decide the convention once, not per-function.
  - [ ] Verify actual C++ compilation. The README's first line promises "c/c++ projects," and `extern "C"`/`ARENA_ALIGN` already account for it, but `CMakeLists.txt` only declares `LANGUAGES C` — nothing has actually  
  compiled this as C++ yet to confirm the claim holds.
  - [ ] Test coverage for `BIT()`/`BIT8/16/32/64()` and the str8/cstr8 macros — everything tested so far is arena-only.
  - [ ] Keep an eye on `ArenaFlags` width (`u8`, room for 4 more flags) — already a known todo in the header; don't let flag #5 become an afterthought.
