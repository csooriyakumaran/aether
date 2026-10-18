# Design: vec2/vec3/vec4 — Fixed-Size Vector Math for aether

Status: **Shipped.** `vec2`/`vec3`/`vec4` and their full op set
(`add`/`sub`/`mul`/`div`/`scale`/`negate`/`dot`/`mag_sq`/`magnitude`/
`normalize`/`lerp`/`eq`/`eq_approx`, plus `vec3_cross`) are implemented in
`aether.h` (version pending release; current working version is
`0.0.20-dev`) and covered by `tests/test_vec_math.c` (12 ctest cases,
`vec_*`, all passing). The `verify_cxx11` conformance-probe regression
(decision 3) is resolved — see its status note for exactly what changed.
`vec4` has since gained an SSE2/NEON SIMD backing (decision 14),
benchmarked in decision 15. This doc is kept as the design record for
*why* the shape looks the way it does — `aether.h` is the source of
truth for current behavior. A draft for the next pass (`mat3`/`mat4`/
quaternions) is appended at the end.

Consumer driving the requirements: the planned graphics platform layer
(windows + OpenGL renderer) — not yet built, but concrete enough in shape
(per-vertex/per-frame transform math, positions, directions, normals,
colors, texture coordinates) to drive real requirements, the same way a
live consumer would. General dynamic-size matrix math (optimization/ML)
is a second, later use case named at the start of this discussion, but
deliberately not driving any decision here — see Scope.

## Motivation

`aether.h` has scalar numeric types (`f32`/`f64`) but nothing between a
scalar and a user-defined struct. A graphics platform layer needs small,
fixed-size vectors with named-component access (position `.xyz`, color
`.rgb`, texture coordinates `.uv`) backing the same flat storage, cheap
enough to pass by value through hot per-vertex/per-frame code.

## Requirements

| # | requirement |
|---|-------------|
| R1 | `f32` storage only in this pass — `f64` variants deferred until a numerical (not graphics) consumer needs the precision |
| R2 | Fixed component counts: 2, 3, 4. No dynamic-size vectors |
| R3 | Named-component access (swizzling) aliasing the same flat storage — no copies, no accessor functions required for the common case |
| R4 | Pure value semantics — no allocation, no lifetime management, stack/register-passable |
| R5 | Dual C11/C++11 header; pedantic-clean (`-pedantic-errors` / `/permissive-`) except one explicitly documented exception (decision 3) |
| R6 | No operator overloading — explicit function API, holds across C and C++ callers (C ABI between modules, same as the rest of aether) |
| R7 | No hidden global state |

## Scope

`vec2`, `vec3`, `vec4` fixed-size float vector types, living in `aether.h`
core (not a separate mythology-named module — this is foundational
numeric utility, not a domain module like `iris`/`hermes`), plus the
arithmetic operations needed for graphics transform math.

**Non-goals (this pass):**
- `mat3`/`mat4`/quaternions — draft sketched at the end of this doc; not
  yet a locked design or implemented.
- General dynamic-size matrix math for optimization/ML. Different data
  shape entirely — arena-backed `{data, rows, cols}`, not a value type —
  and no concrete consumer yet. Per this project's own precedent (`iris`
  and `cereal`/`hermes` were both designed against a real consumer, not
  speculative generality), this stays out until one exists.
- SIMD/intrinsic backing (`__m128`, NEON). Not precluded later — nothing
  here prevents adding it — just not designed for in this pass.
- `f64` variants (`vec3d` or similar — naming TBD when the consumer shows
  up).
- Sub-vector view composition landed sooner than expected: `vec3`/`vec4`
  already carry `xy`/`yz`/`xyz`/`rgb` sub-views (decision 2a below) —
  originally slated as a future maybe, implemented directly instead.

## Decisions and rationale

1. **Lowercase type names: `vec2`/`vec3`/`vec4`.** Matches this project's
   existing case convention — lowercase for trivially-constructible,
   trivially-copyable types with no lifetime to manage (`f32`, `f64`,
   `str8`, `bytes`, `view`); uppercase for types needing init/lifetime
   management (`Arena`, `Thread`, `SerialPort`, `RingBuffer`). A vector is
   firmly in the first category. `mat4`/`quat` follow the same rule (see
   the draft section).

2. **Bare union as the type itself — not a struct wrapping an anonymous
   union.**
   ```c
   union vec3 { f32 e[3]; struct { f32 x, y, z; }; struct { f32 r, g, b; }; };
   ```
   Layout-identical to a struct-wrapping-union (a struct with exactly one
   member and no padding requirement has the same size/alignment as that
   member), so this is purely a declaration-clarity choice, not an ABI
   one. Surveyed prior art converges here: HandmadeMath and RAD Debugger
   (Ryan Fleury / Epic) both declare their vector types as bare unions,
   and Casey Muratori has stated he prefers union-based reinterpretation
   specifically because it documents the intent to reinterpret memory
   clearly — a struct wrapper would obscure that one level down for no
   benefit.

   **2a. Sub-vector views, added during implementation.** `vec3`/`vec4`
   carry extra struct members that alias a smaller vector type at an
   offset, padded with throwaway fields (`_z0`, `_x0`, ...):
   ```c
   union vec3 {
       f32 e[3];
       struct { f32 x, y, z; };
       struct { f32 r, g, b; };
       struct { vec2 xy; f32 _z0; };   /* leading 2 components as a vec2 */
       struct { f32 _x0; vec2 yz; };   /* trailing 2 components as a vec2 */
   };
   ```
   `vec4` adds `xy`, `yz`, `xyz`, and `rgb` the same way. This was
   originally scoped as a maybe-later addition (see the first draft of
   this doc) but landed directly — same prior art (HandmadeMath,
   RAD Debugger) does this too, and the padding-field sizes were verified
   to sum correctly (`vec2` is 8 bytes, `vec3` is 12, `vec4` is 16 — every
   sub-view struct totals to the enclosing type's size with no slack).

3. **Anonymous struct members nested in the union are a named, deliberate
   pedantic-conformance exception.** Standard C11 (ISO C11 §6.7.2.1/13)
   but not standard C++ in any version through C++23 — a GNU/MS extension
   every mainstream compiler supports, which is exactly why HandmadeMath
   and RAD Debugger both rely on it without issue in practice.

   **Status of the test gate — resolved, option (a) from the Validation
   plan.** A clean rebuild of `tests/verify_cxx11.cpp` had failed outright
   (12 `-pedantic-errors` violations, one per anonymous struct across
   `vec2`/`vec3`/`vec4`/`quat`; `ctest` had been reporting a stale
   pre-existing binary as "passed," since ctest does not rebuild
   out-of-date targets itself — caught only by forcing a true rebuild).
   The failure fires from *parsing the union type definitions
   themselves*, not from `verify_cxx11.cpp`'s own body ever constructing
   a `vec3` — so this affected **any** C++ translation unit that merely
   `#include`s `aether.h` under `-pedantic-errors`, whether or not it ever
   touches vector math, not just vec-math call sites.

   Resolved by accepting this as a project-wide, deliberate exception:
   `tests/CMakeLists.txt` now builds `verify_cxx11` with `-pedantic`
   (warn) instead of `-pedantic-errors` (fail) on GCC/Clang —
   `verify_c11` is untouched (C11 has no issue here at all), and MSVC's
   `/permissive-` is untouched on both targets (it never gated this
   extension in the first place; MSVC treats nameless struct/union
   members as a legacy-blessed extension — `C4201` — regardless of
   conformance mode). `verify_cxx11.cpp` now explicitly constructs a
   `vec3` and reads through two different swizzle views
   (`v.r + v.xy.x`), so the probe demonstrably exercises the exception
   it's accepting rather than merely including the header incidentally.
   The pedantic warnings still print during a build — this keeps the
   probe useful for catching anything else non-standard that shows up
   later, without failing over a known, accepted exception.

4. **Reading one named view after writing another (e.g. write `.x`, read
   `.e[0]`) is type punning through a union.** Well-defined in C11
   (common-initial-sequence rule); only informally guaranteed — not
   strictly standard — in C++, where the guarantee is narrower (struct-to-
   struct, not struct-to-array). Accepted as a direct consequence of
   decision 2/3: every reference implementation surveyed relies on exactly
   this, and it's the specific thing Casey Muratori's stated preference
   for unions over casts is endorsing. Verified directly in
   `tests/test_vec_math.c`'s `swizzle_alias` case.

5. **No `{u,v,w}` velocity alias.** Present in the C++ precursor this
   design started from (a CFD library, where `uvw` means velocity), but
   `uv` already means texture coordinates in a graphics-first module —
   reusing it for velocity would collide with the domain this module
   actually serves. Programs with a real velocity-field use case will
   reach for a struct-of-arrays layout rather than an array of `vec3`
   anyway (confirmed when this was discussed — such code wants SoA, not
   an array of this value type, regardless of naming), so there's no
   consumer for the alias here. `vec2` keeps `{u,v}` as texture
   coordinates.

6. **`vec2` keeps `{x,y}` and `{u,v}` only — no `{w,h}` alias.** `x`/`y`
   already read correctly by context as width/height, velocity
   components, or direction length without a dedicated name; a third
   aliased view didn't earn its keep.

7. **Componentwise `_mul`/`_div` included alongside `_scale`.** Real,
   distinct use (color blending, non-uniform scale, UV tiling) from
   vector·scalar scaling.

8. **Both exact (`_eq`) and epsilon-based (`_eq_approx`) equality
   included from the start.** Float equality is a known footgun; shipping
   both avoids every caller hand-rolling its own epsilon compare the first
   time a unit test needs one. `_eq_approx` is Euclidean
   (`mag_sq(sub(b,a)) < epsilon*epsilon`), not per-component — an earlier
   draft had `vec2_eq_approx` as per-component and `vec3_eq_approx` as
   Euclidean simultaneously, an inconsistency caught in review and
   resolved by standardizing on Euclidean across all three ranks, reusing
   `mag_sq`/`sub` rather than hand-expanding components. The comparison is
   strict `<`, so two vectors exactly `epsilon` apart read as *not*
   approximately equal — a deliberate boundary choice, tested directly in
   `tests/test_vec_math.c`'s `eq_approx` case.

9. **No constructor functions (`vec3(x, y, z)` etc.).** Would collide
   with the type's own name — in C, typedef names and function names
   share one namespace, so a function can't be named `vec3` once `vec3`
   is a typedef (contrast `struct stat` / `stat()`, which coexist only
   because the *tag*, not a same-named typedef, is what's distinct).
   Construction is plain aggregate initialization (`vec3 v = {1,2,3};` in
   either language) — free for a trivial aggregate, no header support
   needed. Internal operations build results the same way other value
   types in this header do (`str8_from_view`): a named local, assigned,
   returned — in practice as a loop over `.e[i]` for the component-wise
   ops, matching the shape every `vec{2,3,4}_*` function in the same
   family uses.

10. **Functions use `AETHER_INLINE`, not `AETHER_API`.** `AETHER_INLINE`
    (new macro, `C O N F I G` section) resolves to `static __forceinline`
    on MSVC and `static inline __attribute__((always_inline))` on
    GCC/Clang — a stronger, compiler-forced hint than plain `static
    inline`, matching precedent for small, pure, allocation-free, hot-path
    helpers rather than the declare-in-header/define-in-
    `AETHER_IMPLEMENTATION` split used for heavier, stateful APIs. Landing
    this macro also removed the last bare, unqualified use of `static` in
    the header — `atomic_load_acq_u64`, `atomic_store_rel_u64`,
    `atomic_cas_u64`, and `arena_push_array_` were migrated from plain
    `static inline` to `AETHER_INLINE` in the same pass, so every
    "always-visible, force this to vanish into the call site" function in
    the header now goes through one named macro instead of a spelled-out
    keyword pair repeated at each site — the same motivation as
    `internal`/`global`/`persist` existing for the other meanings of
    `static`. A third preprocessor branch (a fallback for a compiler
    that's neither MSVC nor GCC/Clang) was deliberately **not** added:
    the header already hard-errors on any unsupported compiler at the top
    of the file (`AETHER_COMPILER_MSVC || AETHER_COMPILER_CLANG ||
    AETHER_COMPILER_GCC`, or `#error`), so by the time `AETHER_INLINE` is
    reached, exactly one of those three is already guaranteed `1` — a
    third branch would be unreachable dead code, and a silently-degrading
    fallback would also contradict this header's existing convention of
    `#error`ing loudly on an unsupported configuration rather than
    quietly weakening a guarantee.

11. **`AETHER_SQRTF`/`AETHER_SQRT` macros avoid a `libm` link dependency
    on GCC/Clang entirely, not just on Windows.**
    ```c
    #if AETHER_COMPILER_MSVC
        #include <math.h> /* sqrtf is compiler-recognized under /Oi; CRT math is always linked on Windows */
        #define AETHER_SQRT(x)  sqrt(x)
        #define AETHER_SQRTF(x) sqrtf(x)
    #else
        #define AETHER_SQRT(x)  __builtin_sqrt(x)
        #define AETHER_SQRTF(x) __builtin_sqrtf(x)
    #endif
    ```
    `vec{2,3,4}_magnitude` need a square root. On Linux/glibc, `sqrtf`
    from `<math.h>` historically needs an explicit `-lm` at link time
    (unlike `stdio.h`/`stdlib.h`, which link for free everywhere,
    including POSIX) — a real new link dependency this project's "avoid
    introducing new link/build dependencies" preference flags directly.
    `__builtin_sqrtf`/`__builtin_sqrt` sidestep this completely on
    GCC/Clang: they're true compiler intrinsics, lowered to a hardware
    instruction with no library call at all, not even conditionally. This
    is safe unconditionally (no runtime feature-detection needed) because
    the header's own architecture gate already requires `AETHER_ARCH_X64`
    or `AETHER_ARCH_ARM64` — SSE2 (with its `SQRTSS`/`SQRTSD`) is
    mandatory baseline x86-64, and hardware floating point (`FSQRT`) is
    mandatory baseline AArch64, so hardware sqrt unconditionally exists on
    every target this header allows. `__builtin_*` names are also
    implementation-reserved identifiers, so this introduces no new
    pedantic-conformance exception (unlike decision 3) — calling one is
    ordinary conforming code. MSVC never had the `libm` problem in the
    first place (CRT math is always linked on Windows, and `sqrtf`/`sqrt`
    are already compiler-recognized intrinsics under `/Oi` once
    `<math.h>` is included for the declaration), so that branch is
    unchanged from the obvious approach.

12. **Declared via forward-declare-then-define, not the combined
    one-liner used elsewhere in `aether.h`.**
    ```c
    typedef union vec3 vec3;
    union vec3 { ... };
    ```
    rather than `typedef struct Name { ... } Name;`. Zero functional
    difference — this is purely how the name gets introduced. Adopted for
    two reasons: it's the structural analog of this project's existing
    enum convention (`typedef u8 SerialResult;` names the public thing,
    `enum SerialResult_ {...}` is the separate declaration that fills it
    in), applied to unions instead of integers; and it matches the exact
    prior art (RAD Debugger) this module's union design is drawn from.
    Scoped to new code — not retroactively applied to existing `aether.h`
    types in this pass.

13. **Forward declarations for `vec2`/`vec3`/`vec4` are grouped together**
    near the other core type declarations (next to `bytes`/`view`), full
    unions and functions following later in the `V E C T O R - M A T H`
    section — a section-scoped manifest, not a file-wide one (ImGui's
    style, applied at the size this header's sections actually warrant).

14. **`vec4` gained an SSE2/NEON SIMD backing, isolated behind one internal
    type and a per-op wrapper layer — the same move `AETHER_SQRTF` already
    makes for `sqrt`, one level deeper.**
    ```c
    #if defined(AETHER_NO_SIMD)
        #define AETHER_SIMD_SSE  0
        #define AETHER_SIMD_NEON 0
    #elif AETHER_ARCH_X64
        #define AETHER_SIMD_SSE  1
        #define AETHER_SIMD_NEON 0
    #elif AETHER_ARCH_ARM64
        #define AETHER_SIMD_SSE  0
        #define AETHER_SIMD_NEON 1
    #endif
    ```
    (next to `AETHER_ARCH_X64`/`AETHER_ARCH_ARM64` in `C O N T E X T`).
    `v128_` (internal, trailing-underscore, matching `ThreadStart_`-style
    naming) is `__m128` on SSE2, `float32x4_t` on NEON, or a plain
    `struct { f32 e[4]; }` for the scalar fallback — selected by
    `AETHER_NO_SIMD`, not by hardware capability, since the existing
    architecture gate (`AETHER_ARCH_X64`/`AETHER_ARCH_ARM64`, already a
    hard `#error` otherwise) guarantees one of SSE2/NEON is always present
    on every supported target. `AETHER_NO_SIMD` matches this header's
    existing `AETHER_NO_ASSERT`/`AETHER_NO_ARRAY_COUNT`/`AETHER_NO_MINMAX`
    opt-out convention — for stepping through non-vectorized code in a
    debugger, or comparing scalar/SIMD results against each other.

    Each backend defines the same six op names
    (`v128_add_`/`sub_`/`mul_`/`div_`/`splat_`/`negate_`) plus
    `v128_hsum_` (horizontal sum, needed by `dot`), as `AETHER_INLINE`
    functions — not a mix of macros and functions across branches, same
    reasoning as collapsing `static` into one named macro rather than three
    similar-but-not-identical spellings. `vec4` gains a `.simd` member
    (not underscore-prefixed, unlike the padding fields — it's a legitimate
    reachable view, same spirit as `.xyz`/`.rgb`, for a caller who wants to
    mix raw SSE/NEON code with `aether` vec math directly). Every
    `vec4_add`/`sub`/`mul`/`div`/`scale`/`negate`/`dot`/`mag_sq`/`lerp`
    now reads identically regardless of backend — no `#if` at any call
    site, all three platform differences live in exactly one place.
    `eq`/`eq_approx` stay scalar-loop — `eq` wouldn't benefit without a
    compare+movemask dance for a marginal win on a 4-wide op, and
    `eq_approx` already composes from `mag_sq`/`sub`, which are
    SIMD-backed automatically. `cross` doesn't apply at `vec4` at all.

    **`vec3` is deliberately not backed this way.** `vec4` is exactly 16
    bytes — one lane width, a free fit. `vec3` is 12 bytes; backing it the
    same way means either padding it to 16 (breaking the
    `sizeof(vec3) == 3*sizeof(f32)` guarantee `vec_layout` checks, and the
    "flat array, no hidden padding" principle) or reading 4 floats from a
    12-byte object anyway (`_mm_loadu_ps` past the object's own storage —
    a real out-of-bounds read if a `vec3` is the last thing in a struct or
    allocation, the same bug class already caught once in this vec math
    work). Scalar `vec3`, SIMD `vec4` only.

    **One consequence worth knowing:** adding a 16-byte-aligned member
    raises `alignof(vec4)` from 4 to 16 on the SSE2/NEON branches (still 4
    on the scalar fallback). Dense array packing is unaffected (16 is
    already `vec4`'s own size), but a `vec4` embedded in another struct, or
    on the stack, now needs 16-byte alignment where 4 used to suffice.

    NEON's `v128_hsum_` briefly shipped with a return-type mismatch
    (declared `v128_`/`float32x4_t`, body returning the scalar
    `vaddvq_f32(a)`) — same shape of gap as the `v128_add_`/`v128_negate_`
    issues caught earlier in this work, not caught immediately because
    nothing in this project currently builds for `arm64`. Fixed to
    `f32 v128_hsum_(...)`.

15. **Benchmarked the SIMD backing before trusting it — and the first
    attempt was wrong, instructively so.** Two scratch benchmarks (not
    committed to the repo — they live only in this session's scratchpad;
    if these numbers need reproducing later, they'd need rewriting),
    compiled at `-O3 -DNDEBUG` (matching this project's actual Release
    flags) against the SSE2 path and the scalar fallback (`AETHER_NO_SIMD`).

    The first attempt measured one `vec4`, serially dependent on its own
    previous result each iteration (needed to stop the compiler
    constant-folding the whole loop at `-O3`). It showed SIMD *2–4x
    slower* — because the feedback mechanism wrote a single scalar field
    (`a.x = ...`) and then immediately read the whole object back as a
    128-bit value next iteration: a classic x86 store-forwarding stall
    (a narrow store followed by a wider load from the same address can't
    forward cleanly and stalls waiting for the store to retire) that only
    exists in the SIMD build — the scalar fallback never does a 128-bit
    load at all, so it never pays this cost. Rewriting the feedback to
    stay entirely in vector space (`vec4_scale` instead of a direct field
    write) removed the artifact: corrected single-value timings landed
    within noise of each other (SIMD sometimes a few percent slower, never
    meaningfully faster) across `add`/`mul`/`dot`/a mixed op sequence.

    A second benchmark processing 65,536 *independent* `vec4`s per pass
    (2000 passes, no serial dependency between elements — the array itself
    gives the compiler enough real work that constant-folding isn't a
    risk) told the opposite, consistent story: **SIMD ~2.4–2.7x faster**
    across `add`, a multiply-add "integrate" step, and a dot-product-driven
    update, stable across three repeated runs each. The scalar build for
    this comparison was compiled with `-fno-tree-vectorize
    -fno-tree-slp-vectorize` specifically so GCC's own auto-vectorizer
    couldn't quietly turn the "scalar" baseline into SSE code too, which
    would have hidden the real comparison.

    **The takeaway, and why both results are correct rather than
    contradictory:** SIMD's advantage comes from genuine data parallelism
    — many independent values the CPU can have in flight at once — not
    from speeding up the latency of one value in isolation. Modern x86
    cores have enough independent scalar FP execution ports that four
    scalar ops with no dependency between them aren't meaningfully slower
    than one SSE instruction; the 4-wide win only shows up operating on a
    batch. This matters directly for how `mat4`'s eventual
    `mat4_mul_vec4` gets used: applied to one vertex at a time in a naive
    per-vertex loop, this backing may show nothing; applied to a real
    vertex/particle batch, it's where the ~2.5x actually shows up. Not a
    decision to act on now — just the reason to reach for a batched
    transform API later if performance ever becomes the actual goal,
    rather than assuming the per-vertex call site is where the SIMD work
    pays for itself.

## Public API

```c
/* forward declarations, grouped with the other core types */
typedef union vec2 vec2;
typedef union vec3 vec3;
typedef union vec4 vec4;

/* definitions */
union vec2 { f32 e[2]; struct { f32 x, y; }; struct { f32 u, v; }; };
union vec3 {
    f32 e[3];
    struct { f32 x, y, z; };
    struct { f32 r, g, b; };
    struct { vec2 xy; f32 _z0; };
    struct { f32 _x0; vec2 yz; };
};
union vec4 {
    f32 e[4];
    struct { f32 x, y, z, w; };
    struct { f32 r, g, b, a; };
    struct { vec2 xy; f32 _z0, _w0; };
    struct { f32 _x1; vec2 yz; f32 _w1; };
    struct { vec3 xyz; f32 _w2; };
    struct { vec3 rgb; f32 _a0; };
};

AETHER_INLINE f32 f32_lerp(f32 a, f32 b, f32 t); /* shared scalar helper, used by every vec*_lerp */

/* per-type, vec{2,3,4}_*: */
AETHER_INLINE vec3 vec3_add(vec3 a, vec3 b);
AETHER_INLINE vec3 vec3_sub(vec3 a, vec3 b);
AETHER_INLINE vec3 vec3_mul(vec3 a, vec3 b);        /* componentwise */
AETHER_INLINE vec3 vec3_div(vec3 a, vec3 b);        /* componentwise */
AETHER_INLINE vec3 vec3_scale(vec3 a, f32 s);
AETHER_INLINE vec3 vec3_negate(vec3 a);
AETHER_INLINE f32  vec3_dot(vec3 a, vec3 b);
AETHER_INLINE f32  vec3_mag_sq(vec3 a);
AETHER_INLINE f32  vec3_magnitude(vec3 a);          /* AETHER_SQRTF(mag_sq) */
AETHER_INLINE vec3 vec3_normalize(vec3 a);          /* AETHER_ASSERT_s mag_sq > 0 */
AETHER_INLINE vec3 vec3_lerp(vec3 a, vec3 b, f32 t);
AETHER_INLINE b8   vec3_eq(vec3 a, vec3 b);          /* exact */
AETHER_INLINE b8   vec3_eq_approx(vec3 a, vec3 b, f32 epsilon); /* Euclidean */

/* vec3 only: */
AETHER_INLINE vec3 vec3_cross(vec3 a, vec3 b);
```
`vec2_*`/`vec4_*` mirror the same shape (no `cross` at other ranks). `f32`/
`b8` from `aether.h` core. Naming mirrors `str8_*`/`arena_*` (noun_verb);
no operator overloading in either language, so the same calls work
identically for C and C++ callers.

## Usage sketch

```c
vec3 a = {1.0f, 0.0f, 0.0f};
vec3 b = {0.0f, 1.0f, 0.0f};

vec3 n = vec3_normalize(vec3_cross(a, b));   /* {0,0,1} */
f32  d = vec3_dot(a, b);                      /* 0 */

vec3 color = {0};
color.r = 0.2f; color.g = 0.4f; color.b = 0.8f;   /* same storage as .xyz */

vec3 start = {0,0,0}, end = {10,0,0};
vec3 mid   = vec3_lerp(start, end, 0.5f);         /* {5,0,0} */
```

## Pitfalls to preserve when implementing

These are no longer hypothetical — every one below is a bug that actually
shipped during this implementation and was caught in review, not a
speculative risk:

- **A component-wise op's loop bound must match its own rank, not get
  copy-pasted from a neighboring rank.** `vec2_add`/`sub`/`mul`/`div` all
  shipped first with `for (u64 i = 0; i < 4; ++i)` — a real out-of-bounds
  read/write 8 bytes past the end of an 8-byte `vec2`, not just a wrong
  value, caught by comparing each function's loop bound against its own
  type's element count one at a time.
- **A same-shaped sibling function's signature can silently drift.**
  `vec4_lerp` shipped with `vec4 vec4_lerp(vec4 a, vec3 b, f32 t)` — second
  parameter the wrong type, a straight copy-paste miss. Caught the same
  way: checking every parameter's type against the function's own name,
  not assuming a sibling function was transcribed correctly.
- **The dot-product typo (`+` where `*` belongs) recurred at a different
  rank after being fixed once.** `vec3_dot` shipped with `a.y+b.y`
  instead of `a.y*b.y`; after that was fixed, `vec4_dot` shipped with the
  identical mistake on its `w` term. Both are silent — no crash, just a
  wrong number that cascades into `mag_sq`/`magnitude`/`normalize`,
  caught only by checking the actual arithmetic against the formula by
  hand rather than trusting the shape looked right.
- **`cross` is `vec3`-only.** Don't add a same-named function at other
  ranks that silently does something else (e.g. a 2D "pseudo-cross"
  returning a scalar) — if that's ever wanted, name it distinctly
  (`vec2_perp_dot` or similar), don't overload `cross`'s meaning.
- **`eq_approx`'s `epsilon` is a parameter, never a hardcoded constant**
  — the right tolerance is call-site-dependent.
- **`normalize` on a zero-length input is a decided case** (`AETHER_ASSERT_`
  in debug, `NaN` fallthrough in release — see decision 11's sibling
  discussion and `tests/test_vec_math.c`'s `normalize_zero_vector` case),
  not an accident of what the division happens to do. Internal invariant
  checks like this use `AETHER_ASSERT_` directly, never the public
  `ASSERT` — `ASSERT` is `#ifndef`-guarded so a consumer's own
  pre-defined `ASSERT` macro can shadow it, and the library's own
  correctness checks must not inherit a consumer's unrelated assert
  semantics. Every other internal check in the header already follows
  this (`arena_push`, `align_forward_u64_`, ...); `vec*_normalize` matches
  it, not a new pattern.
- **`f32_lerp` must use the endpoint-exact form, `(1-t)*a + t*b`, not
  `a + t*(b-a)`.** The latter only guarantees the `t=0` endpoint exactly
  in floating point (multiplying by exact `0.0f` is exact); `t=1` can
  land a ULP or two off `b`. Shipped first with the imprecise form,
  caught as a numerical-stability review note rather than a crash.

## Validation plan

Implemented as `tests/test_vec_math.c`, 12 cases registered in `ctest`
under the `vec_` prefix (`vec_layout`, `vec_swizzle_alias`,
`vec_elementwise`, `vec_scalar_ops`, `vec_dot`, `vec_cross`,
`vec_magnitude`, `vec_normalize`, `vec_normalize_zero_vector`, `vec_lerp`,
`vec_eq`, `vec_eq_approx`), matching this header's existing
behavior-named (not type-named) test-case convention.

1. **Layout** (`vec_layout`): `sizeof(vec{2,3,4}) == N * sizeof(f32)` —
   guards against unexpected padding.
2. **Swizzle aliasing** (`vec_swizzle_alias`): write through one named
   view, read through `.e[]` and every other alias (including the `xy`/
   `yz`/`xyz`/`rgb` sub-views) — confirms every view actually shares
   storage.
3. **Arithmetic** (`vec_elementwise`, `vec_scalar_ops`, `vec_dot`,
   `vec_cross`): each op against hand-computed values, chosen to stay
   exact under `==` (integer-valued components, divisions that come out
   even) rather than needing a tolerance; `cross` additionally checked
   against the right-handed basis, anti-commutativity, and self-cross.
4. **Magnitude/normalize** (`vec_magnitude`, `vec_normalize`): Pythagorean
   triples for exact `mag_sq`, a tolerance-based check on `magnitude`/
   `normalize` (the one place `AETHER_SQRTF` makes bit-exact `==`
   unreasonable), and a direction-preserved-under-positive-scale check.
5. **Normalize's zero-vector contract** (`vec_normalize_zero_vector`):
   gated on `AETHER_ENABLE_ASSERTS` exactly like `test_arenas.c`'s
   `overflow_guard` — prints a skip notice in a debug build (where the
   assert would otherwise trap the test process itself) and exercises the
   release-mode `NaN` fallthrough (`n.x != n.x`) otherwise. Verified by
   hand in both a Debug and a Release build during this implementation —
   both paths actually run as intended, not just the one the default
   build happens to hit.
6. **Lerp** (`vec_lerp`): `t=0`/`t=1` exact-endpoint checks (this is what
   caught the `f32_lerp` formula needing to change), plus a midpoint
   value.
7. **Equality** (`vec_eq`, `vec_eq_approx`): exact equality including a
   full-loop exercise (components equal except the last one); approx
   equality inside/outside tolerance, and the exact-epsilon boundary
   pinned to the current strict-`<` behavior (decision 8).
8. **Conformance gate — resolved.** Option (a) was chosen: accept that
   `aether.h` is no longer pedantic-C++11-clean as a whole (a project-wide
   posture change, not a vec-math-local one — the anonymous-struct
   extension is universally supported in practice across MSVC/GCC/Clang),
   and downgrade `verify_cxx11`'s GCC/Clang flag from `-pedantic-errors`
   to `-pedantic` rather than revisit decision 2/3's union design.
   Mechanics in decision 3's status note. Option (b) (the named,
   tagged-sub-struct alternative — `v.pos.x`/`v.col.r` instead of bare
   `v.x`/`v.r`) remains available later if this exception is ever judged
   to have been the wrong call, but isn't the path taken.

## Follow-ups after landing

- `mat3`/`mat4`/quaternion design — drafted below; not yet a locked
  design or implemented.
- Sub-vector view composition landed already (decision 2a) — no longer a
  follow-up.
- The `verify_cxx11` conformance-gate gap (Validation plan item 8) is
  resolved — no longer a follow-up.
- `vec4`'s SSE2/NEON SIMD backing is now written up (decision 14) and
  benchmarked (decision 15) — no longer a follow-up.
- A batched/array transform API, motivated directly by decision 15's
  benchmark finding (SIMD only pays off with independent, batchable
  data) — not designed, just named as the place this is headed if
  per-vertex throughput ever becomes a real goal.
- `f64` vector variants if a numerical (non-graphics) consumer needs the
  precision — naming TBD at that point.
- Promotion out of `aether.h` core into its own module is not anticipated
  — this is foundational numeric utility, not a domain module, and is
  staying in core by design (unlike `iris`/`hermes`, which are
  domain-specific and mythology-named).

---

## Draft: `mat4`/`mat3`/`quat` for graphics (not yet designed)

This section sketches a starting point for the next real design pass —
nothing below is locked. It exists so the next conversation has a
concrete draft to react to instead of a blank page, in the same spirit as
how this doc itself started. Treat every signature here as a proposal,
not a decision.

### Why matrices and quaternions are harder than vectors

Vectors had one real fork to resolve (fixed-size graphics type vs.
dynamic optimization/ML matrix — Scope already settled this in favor of
the former). Matrices and quaternions have several more, each consequential
enough to warrant its own discussion before any code:

1. **Row-major vs. column-major storage.** OpenGL's own convention
   (column-major, column-vector transforms, `v' = M * v`) is the natural
   default for a module whose first consumer is an OpenGL renderer — but
   it's a real decision, not a default to assume silently, since it
   determines how `e[16]` maps to `m[row][col]` and how a `mat4` uploads
   to a shader uniform without a transpose.
2. **Should `mat4` get the same bare-union-with-swizzle treatment as
   `vec4`?** A matrix doesn't need named-component access the way a
   vector does (`m.xy` means nothing), but a `vec4 col[4]` view over the
   same storage as `f32 e[16]` is exactly the sub-vector-view trick
   `vec3`/`vec4` already use (decision 2a) — and genuinely useful, since
   graphics code frequently wants "column 2 as a vec4" for a
   matrix-vector multiply's inner loop:
   ```c
   union mat4 { f32 e[16]; vec4 col[4]; f32 m[4][4]; };
   ```
   This is a real candidate, not a certainty — worth deciding deliberately
   rather than copying the vector pattern by reflex.
3. **How much of `inverse` is in scope.** A general 4×4 inverse is
   expensive and rarely what graphics code actually needs — most matrices
   in a rendering pipeline are affine (the bottom row is `[0,0,0,1]`), and
   a fast affine-only inverse is a different, much cheaper function than
   a general one. Shipping only the general form because it's "more
   correct" would be the kind of premature generality this project
   otherwise avoids; shipping only the affine-fast form without naming
   that restriction would be a silent footgun. Needs an explicit choice,
   probably both under distinct names.
4. **Does `quat` reuse `vec4`'s layout, or stay a fully distinct type?**
   Same four `f32`s, but quaternion multiplication isn't vector dot
   product, and conflating the two types invites calling `vec4_dot` where
   `quat_mul` was meant, with no compiler error to catch it. Prior art
   (HandmadeMath) keeps a quaternion type structurally identical to
   `vec4` but nominally distinct — likely the right call here too, but
   worth confirming rather than assuming.

### A starting sketch (not a decision)

```c
typedef union mat4 mat4;
union mat4 { f32 e[16]; vec4 col[4]; f32 m[4][4]; };

AETHER_INLINE mat4 mat4_identity(void);
AETHER_INLINE mat4 mat4_mul(mat4 a, mat4 b);
AETHER_INLINE vec4 mat4_mul_vec4(mat4 m, vec4 v);
AETHER_INLINE mat4 mat4_transpose(mat4 m);
AETHER_INLINE mat4 mat4_affine_inverse(mat4 m); /* fast path: bottom row [0,0,0,1] assumed */
AETHER_INLINE mat4 mat4_inverse(mat4 m);        /* general, slower */

/* construction helpers a renderer actually calls directly */
AETHER_INLINE mat4 mat4_translate(vec3 t);
AETHER_INLINE mat4 mat4_scale(vec3 s);
AETHER_INLINE mat4 mat4_rotate(vec3 axis, f32 angle_rad);
AETHER_INLINE mat4 mat4_perspective(f32 fov_y_rad, f32 aspect, f32 z_near, f32 z_far);
AETHER_INLINE mat4 mat4_orthographic(f32 left, f32 right, f32 bottom, f32 top, f32 z_near, f32 z_far);
AETHER_INLINE mat4 mat4_look_at(vec3 eye, vec3 target, vec3 up);

typedef union quat quat;
union quat { f32 e[4]; struct { f32 x, y, z, w; }; struct { vec3 xyz; f32 _w0; }; };

AETHER_INLINE quat quat_identity(void);
AETHER_INLINE quat quat_from_axis_angle(vec3 axis, f32 angle_rad);
AETHER_INLINE quat quat_mul(quat a, quat b);          /* Hamilton product, not componentwise */
AETHER_INLINE quat quat_conjugate(quat q);
AETHER_INLINE quat quat_normalize(quat q);            /* same shape as vec4_normalize */
AETHER_INLINE vec3 quat_rotate_vec3(quat q, vec3 v);
AETHER_INLINE quat quat_slerp(quat a, quat b, f32 t);
AETHER_INLINE mat4 quat_to_mat4(quat q);
```

`mat3` isn't sketched here at all — it's less obviously needed than
`mat4` for a first OpenGL renderer (normal-matrix transforms are the main
use, and those can start as "the upper-left 3×3 of a `mat4`" without a
distinct type). Whether it earns its own type is itself an open question
for the real design pass, not a yes assumed here.

### Open questions for next time

- Row-major vs. column-major (point 1 above) — settle this first, since
  it affects every other signature's internal layout.
- Whether `mat4`'s union/swizzle treatment (point 2) is worth it, given
  matrices don't need named-component access the way vectors do.
- `inverse` scope (point 3) — affine-fast, general, or both under
  distinct names.
- `quat` as its own type vs. reusing `vec4`'s layout (point 4).
- Whether any of this should use `AETHER_SQRTF`-style compiler builtins
  for trig (`sin`/`cos` for `mat4_rotate`/`mat4_perspective`) the same way
  `magnitude` does for `sqrt` — likely yes, same reasoning, but not yet
  confirmed against what GCC/Clang/MSVC actually expose as builtins for
  `sin`/`cos` specifically.
