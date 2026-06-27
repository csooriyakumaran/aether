/* Compile-only conformance probe: the PUBLIC header must be valid C++11 with no
   extensions. See verify_c11.c for why the implementation is not included. */
#include "aether/aether.h"

int main()
{
    bytes      b   = {0};
    bytes_view bv  = view_from_bytes(b);
    str8       s   = {0};
    str8_view  sv  = view_from_str8(s);
    str8_view  lit = STR("probe");   /* str8_view{...} braced temporary -> C++11 */
    u64        a   = (u64)ARENA_ALIGN(double);  /* alignof(double) */

    (void)bv; (void)sv; (void)lit; (void)a;
    return 0;
}
