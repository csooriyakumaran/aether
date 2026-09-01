/* Compile-only conformance probe: the PUBLIC header must be valid C++11 with no
   extensions. See verify_c11.c for why the implementation is not included. */
#include "aether/aether.h"

int main()
{
    bytes b   = {0};
    str8  s   = {0};
    str8  lit = STR("probe");   /* str8{...} braced temporary -> C++11 */
    u64   a   = (u64)ARENA_ALIGN(double);  /* alignof(double) */

    (void)b; (void)s; (void)lit; (void)a;
    return 0;
}
