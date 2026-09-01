/* Compile-only conformance probe: the PUBLIC header must be valid C11 with no
   extensions. AETHER_IMPLEMENTATION is intentionally NOT defined -- its Windows
   backend includes <windows.h>, which is not -pedantic clean. This guards the
   surface every consumer compiles: declarations, ARENA_ALIGN (_Alignof), and
   the STR macro. */
#include "aether/aether.h"

int main(void)
{
    bytes b   = {0};
    str8  s   = {0};
    str8  lit = STR("probe");
    u64   a   = (u64)ARENA_ALIGN(double);

    (void)b; (void)s; (void)lit; (void)a;
    return 0;
}
