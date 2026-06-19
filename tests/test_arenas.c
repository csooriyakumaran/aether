#define AETHER_IMPLEMENTATION
#include "aether/aether.h"

int main(void)
{
    Arena arena = arena_alloc(MB(1));

    u64 p0 = arena.pos;
    u32* x = arena_push_t(&arena, u32);
    ASSERT(x != 0);
    ASSERT(arena.pos > p0);

    arena_clear(&arena);
    ASSERT(arena.pos == 0);

    arena_release(&arena);
    ASSERT(arena.base == 0);



    return 0;
}
