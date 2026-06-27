/* Compile-only conformance probe: the PUBLIC header must be valid C11 with no
   extensions. AETHER_IMPLEMENTATION is intentionally NOT defined -- its Windows
   backend includes <windows.h>, which is not -pedantic clean. This guards the
   surface every consumer compiles: declarations, the inline view helpers,
   ARENA_ALIGN (_Alignof), and the STR macro. */
#include "aether/aether.h"

int main(void)
{
    bytes      b   = {0};
    bytes_view bv  = view_from_bytes(b);
    str8       s   = {0};
    str8_view  sv  = view_from_str8(s);
    str8_view  lit = STR("probe");
    u64        a   = (u64)ARENA_ALIGN(double);

    (void)bv; (void)sv; (void)lit; (void)a;
    return 0;
}
