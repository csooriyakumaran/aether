
#define AETHER_IMPLEMENTATION
#include "aether/aether.h"

#include <stdio.h>
#include <inttypes.h>

#define PAGESIZE 4096
#define CACHELINE 64
#define DUMP_BYTES_PER_ROW 16


static void print_binary_u8(u8 value)
{
    printf("0b");
    for (i32 i = 7; i >= 0; --i)
    {
        // if ((i+1) % 4 == 0) printf(" ");
        printf("%c", (value & (1u << i)) ? '1' : '0');
    }
}

static void print_binary_u64(u64 value)
{
    printf("0b");
    for (i32 i = 63; i >= 0; --i)
    {
        // if ((i+1) % 4 == 0) printf(" ");
        printf("%c", (value & (1ull << i)) ? '1' : '0');
    }
}

static void arena_hexdump(Arena* arena)
{
    if (!arena->base) return;

    u64 n = arena->pos;


    printf("------  ARENA --------------------\n");
    printf(" Base Address : 0x%016llX\n",    (unsigned long long)(uintptr_t)arena->base);
    printf("     Reserved : %12llu bytes\n", (unsigned long long)arena->reserved_size);
    printf("    Committed : %12llu bytes\n", (unsigned long long)arena->commit_size);
    printf("     Position : %12llu bytes\n", (unsigned long long)arena->pos);
    printf("        Flags :         "); print_binary_u8(arena->flags); printf("\n");
    printf("----------------------------------\n");
    printf("sizeof(Arena) : %12llu bytes\n", sizeof(Arena));
    printf("==================================\n");

    printf("\n");
    printf(" OFFSET    BYTES                                             ASCII\n");
    printf("--------  ------------------------------------------------  ------------------\n");

    for (u64 offset = 0; offset < n; offset += DUMP_BYTES_PER_ROW)
    {
        if (offset > 0 && (offset % PAGESIZE) == 0)
        {
            printf("--------  ---------------- page boundary -----------------  ------------------\n");
        }
        else if (offset > 0 && (offset % CACHELINE) == 0)
        {
            printf("--------  ---------------- cache line --------------------  ------------------\n");
        }

        printf("%08llx  ", (unsigned long long)offset);

        for (u64 i = 0; i < DUMP_BYTES_PER_ROW; ++i)
        {
            if (offset + i < n)
            {
              printf("%02x ", arena->base[offset+i]);
            }
            else
            {
                printf("   ");
            }
            if (i == 7)
            {
                printf(" ");
            }
        }

        printf(" |");

        for (u64 i = 0; i < DUMP_BYTES_PER_ROW; ++i)
        {
            if (offset + i < n)
            {
                u8 c = arena->base[offset + i];
                printf("%c", (c >= 32 && c <= 126) ? c : '.');
            }
            else
            {
                printf(" ");
            }
        }

        printf("|\n");

    }
    printf("========  ================================================  ==================\n");
}


int main(void)
{

    // - VERSION
    printf("--- VERSION -------------------------\n");
    printf("AETHER: v%s\n", AETHER_VERSION_STRING);
    printf("  Major: %d\n", AETHER_VERSION_MAJOR);
    printf("  Minor: %d\n", AETHER_VERSION_MINOR);
    printf("  Patch: %d\n", AETHER_VERSION_PATCH);
    printf("\n");

    // - TYPES

    // - STRINGS
    printf("--- STRINGS ------------------------\n");
    cstr8 string_literal = STR("Hello, World");
    str8 mutable = {
        .data = (u8*)"abcdef",
        .size = 6
    };
    printf("String literal:\t" STR8_FMT "\n", STR8_ARG(string_literal));
    printf("Mutable string:\t" STR8_FMT "\n", STR8_ARG(mutable));
    printf("\n");

    // - BIT FIELDS

    printf("--- BIT FIELDS ---------------------\n");
    typedef u8 BitField;
    enum BitField_{
        BitField_None   = 0,
        BitField_One    = BIT(0),
        BitField_Two    = BIT(1),
        BitField_Three  = BIT(2),
        BitField_Four   = BIT(3),
        BitField_Five   = BIT(4),
        BitField_Six    = BIT(5),
        BitField_Seven  = BIT(6),
        BitField_Eight  = BIT(7),
        BitField_OneAndEight = BitField_One | BitField_Eight,
        BitField_All    = BIT(0) | BIT(1) | BIT(2) | BIT(3) | BIT(4) | BIT(5) | BIT(6) | BIT(7)
    };

    printf("           NONE = "); print_binary_u8(BitField_None);        printf("\n");
    printf("         BIT(0) = "); print_binary_u8(BitField_One);         printf("\n");
    printf("         BIT(1) = "); print_binary_u8(BitField_Two);         printf("\n");
    printf("         BIT(2) = "); print_binary_u8(BitField_Three);       printf("\n");
    printf("         BIT(3) = "); print_binary_u8(BitField_Four);        printf("\n");
    printf("         BIT(4) = "); print_binary_u8(BitField_Five);        printf("\n");
    printf("         BIT(5) = "); print_binary_u8(BitField_Six);         printf("\n");
    printf("         BIT(6) = "); print_binary_u8(BitField_Seven);       printf("\n");
    printf("         BIT(7) = "); print_binary_u8(BitField_Eight);       printf("\n");
    printf("BIT(1) | BIT(7) = "); print_binary_u8(BitField_OneAndEight); printf("\n");
    printf("            ALL = "); print_binary_u8(BitField_All);         printf("\n");
    printf("\n"); 

    // - ARENAS
    printf("--- ARENAS -------------------------\n");
    printf("\n");
    printf("--- Alloc : reserve KB(64), commit KB(0)\n");
    printf("--- flags : ArenaFlags_Decommit |\n");
    printf("            ArenaFlags_DebugFillOnClear\n");
    printf("\n");
    Arena arena = arena_alloc(KB(64), 0);
    arena.flags = ArenaFlags_Decommit | ArenaFlags_DebugFillOnClear;
    arena_hexdump(&arena);

    printf("\n");
    printf("--- Push : u32, char[5], u64[10], vec2[10]\n");
    printf("---      : u32    -> 0x12345678\n");
    printf("---      : char[] -> `hello'\n");
    printf("---      : save position\n");
    printf("---      : u64[]  -> {0 .. 9}\n");
    printf("---      : vec2[] -> {0}\n");

    printf("\n");

    u32* a = arena_push_t(&arena, u32);
    *a = 0x12345678;

    cstr8 hello = STR("hello");
    u8* text = arena_push_array(&arena, u8, hello.size);
    memcpy(text, hello.data, hello.size);
    u64 pos = arena.pos;

    u64* data = arena_push_array(&arena, u64, 10);
    for (u64 i = 0; i < 10; ++i)
        data[i] = i;


    typedef struct {
        f64 x;
        f64 y;
    } test_vec2;

    test_vec2* vecs = arena_push_array(&arena, test_vec2, 10);

    arena_hexdump(&arena);
    printf("\n");
    printf("--- pop : to saved position\n");
    printf("\n");
    arena_pop_to(&arena, pos);
    arena_hexdump(&arena);
    u32* new_data = arena_push_array_nozero(&arena, u32, 20);
    printf("\n");
    printf("--- push : u32[20] (nozero)\n");
    printf("---      : unitialized, old u64 data still lives in memory\n");
    printf("---      : u32[0] -> %d\n", new_data[0]);
    printf("\n");

    arena_hexdump(&arena);


    printf("\n");
    printf("--- clear : Decommitting due to flags\n");
    printf("\n");
    arena_clear(&arena);

    arena_hexdump(&arena);






    return 0;
}
