/* Compile-only conformance probe: the PUBLIC header must be valid C++11, with
   one documented, deliberate exception -- vec2/vec3/vec4's swizzle unions use
   anonymous struct members (standard C11, not standard C++; see
   docs/vec-math-design.md decision 3), which is why this probe builds under
   -pedantic (warn) rather than -pedantic-errors (fail) on GCC/Clang. Every
   other non-standard usage should still fail this probe.
   See verify_c11.c for why the implementation is not included. */
#include "aether/aether.h"

int main()
{
    bytes b   = {0};
    str8  s   = {0};
    str8  lit = STR("probe");   /* str8{...} braced temporary -> C++11 */
    u64   a   = (u64)ARENA_ALIGN(double);  /* alignof(double) */

    vec3 v = {1.0f, 2.0f, 3.0f};
    f32  swizzled = v.r + v.xy.x;   /* exercises the anonymous-struct exception directly */

    (void)b; (void)s; (void)lit; (void)a; (void)swizzled;
    return 0;
}
