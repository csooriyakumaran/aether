
#define AETHER_IMPLEMENTATION
#include "aether/aether.h"
#include "aether/aether-version.h"

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

static void print_pad(u64 n) { for (u64 i = 0; i < n; ++i) putchar(' '); }
static void print_ring_buffer(RingBuffer* rb)
{
    const u64 pad   = 2;
    const u64 count = 16;
    const u64 cellw = 5; /* "0x00 " */
    const u64 lead  = 6; /* "[ ... " */
    const u64 seamw = 2; /* "| " */

    u64 mask = rb->size - 1;
    u64 lo   = rb->read - pad;

    u64 seam_cell  = (rb->size - (lo & mask)) & mask;
    b8  seam_shown = seam_cell < count;

    u64 r_cell = pad;
    u64 w_cell = rb->write - lo;
    b8  w_vis  = (w_cell < count);

    #define COL_OF(c) (lead + (c)*cellw + ((seam_shown && seam_cell <= (c)) ? seamw : 0))

    printf("=============================================================================================\n");
    if (w_vis)
    {
        print_pad(COL_OF(w_cell)); printf("W(%llu)\n", (unsigned long long)rb->write);
        print_pad(COL_OF(w_cell)); printf("v\n");
    } else {
        u64 width = lead + count*cellw + (seam_shown ? seamw : 0) + 5;
        char lbl[40];
        snprintf(lbl, sizeof(lbl), "W(%llu) ->", (unsigned long long)rb->write);
        u64 c = width > (u64)strlen(lbl) ? width - (u64)strlen(lbl) : 0;
        printf("\n");
        print_pad(c); printf("%s\n", lbl);
    }

    printf("[ ... ");
    for (u64 i = 0; i < count; ++i)
    {
        if (seam_shown && i == seam_cell) printf("| ");
        printf("0x%02X ", rb->base[(lo + i) & mask]);
    }

    printf("... ]\n");

    print_pad(COL_OF(r_cell)); printf("^\n");
    print_pad(COL_OF(r_cell)); printf("R(%llu)\n", (unsigned long long)rb->read);

    printf("=============================================================================================\n");
    #undef COL_OF
}


static void arena_hexdump(Arena* arena)
{
    if (!arena->base) return;

    u64 n = arena->pos;


    printf("------  ARENA ------------------------------\n");
    printf("       Base Address :     0x%016llX\n",    (unsigned long long)(uintptr_t)arena->base);
    printf("    Reserved Memory : %16llu bytes (%.2f MB)\n", (unsigned long long)arena->reserved_size, (f64)arena->reserved_size / MB(1));
    printf("   Committed Memory : %16llu bytes (%.2f KB)\n", (unsigned long long)arena->commit_size, (f64)arena->commit_size / KB(1));
    printf("           Position : %16llu bytes\n", (unsigned long long)arena->pos);
    printf("              Flags :             "); print_binary_u8(arena->flags); printf("\n");
    printf(" Commit Granularity : %16u pages\n", arena->granularity);
    printf("--------------------------------------------\n");
    printf("sizeof(Arena) : %16llu bytes\n", sizeof(Arena));
    printf("============================================\n");

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
    str8_view string_literal = STR("Hello, World");
    str8 mutable = {
        .data = (u8*)"abcdef",
        .size = 6
    };
    printf("String literal:\t" STR8_FMT "\n", STR8_ARG(string_literal));
    printf("Mutable string:\t" STR8_FMT "\n", STR8_ARG(mutable));
    printf("\n");

    // - UNITS 
    printf("--- UNIT MACROS ---------------------\n");
    printf("KB(1) = %14llu bytes\n", KB(1));
    printf("MB(1) = %14llu bytes\n", MB(1));
    printf("GB(1) = %14llu bytes\n", GB(1));
    printf("TB(1) = %14llu bytes\n", TB(1));
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
    printf("--- Alloc : reserve MB(4)");
    printf("\n");
    Arena arena = arena_alloc(MB(4));
    arena_hexdump(&arena);

    u32* a = arena_push_t(&arena, u32);
    *a = 0x12345678;

    /* no-zero terminiation */
    str8 hello = arena_push_str8_copy(&arena, STR("hello") );
    /* with terminiation */
    const char* world = arena_push_cstring(&arena, ", world!");

    str8 fmt_str8 = arena_push_str8_fmt(&arena, "fmt %s %d", "str8: a = ", *a);
    const char* fmt_char = arena_push_cstring_fmt(&arena, "fmt %s %d", "char*: a = ", *a);

    u64 mark1 = arena.pos;

    u64* data = arena_push_array(&arena, u64, 10);
    for (u64 i = 0; i < 10; ++i)
        data[i] = i;


    typedef struct {
        f64 x;
        f64 y;
    } test_vec2;

    test_vec2* vecs = arena_push_array(&arena, test_vec2, 10);
    u64 mark2 = arena.pos;

    printf("\n");
    printf("--- Push : u32      -> 0x12345678\n");
    printf("--- Push : st8      -> `hello'\n");
    printf("--- Push : char*    -> `, world!`\n");
    printf("--- Push : st8      -> formatted\n");
    printf("--- Push : char*    -> frmatted\n");
    printf("   mark1 : save position (%llu)\n", mark1);
    printf("--- Push : u64[10]  -> {0 .. 9}\n");
    printf("--- Push : vec2[10] -> {0}\n");
    printf("   mark2 : save position (%llu)\n", mark2);
    printf("\n");

    arena_hexdump(&arena);

    arena_pop_to(&arena, mark1);
    printf("\n");
    printf("--- pop : to saved position (%llu)\n", mark1);
    printf("        : decommit and fill dependent on flags, check below ..\n");
    printf("\n");
    printf("arena.base[%3llu .. %4llu] = { ", mark1-1, mark2-1);
    for (u64 i = 0; i < 5; ++i)
        printf("0x%02X ", arena.base[arena.pos+i]);
    printf(" ... 0x%02X }\n", arena.base[mark2-1]);
    printf("arena.base[%3llu .. %4llu] = { ", mark2, arena.commit_size-1);
    for (u64 i = 0; i < 5; ++i)
        printf("0x%02X ", arena.base[mark2+i]);
    printf(" ... 0x%02X }\n", arena.base[arena.commit_size-1]);
    printf("\n");

    arena_hexdump(&arena);

    u32* new_data = arena_push_array(&arena, u32, 20);
    printf("\n");
    printf("--- push : u32[20]\n");
    printf("---      : u32[0] -> %d\n", new_data[0]);
    printf("\n");

    arena_hexdump(&arena);


    printf("\n");
    printf("--- clear : Decommitting due to flags\n");
    printf("\n");
    arena_clear(&arena);

    arena_hexdump(&arena);


    RingBuffer* buffer = arena_push_t_zero(&arena, RingBuffer);
    if(!ring_buffer_alloc(buffer, KB(64)))
    {
        fprintf(stderr, "Could not allocate ring buffer\n");
    }

    printf("\n");
    printf("--- RING BUFFERS ----------------------\n");
    printf("\n");

    printf("--- BUFFER ALLOCATION\n");
    printf("--- Base Address   : 0x%016llX\n",    (unsigned long long)(uintptr_t)buffer->base);
    printf("--- Allocated size : %12llu bytes (%.2f MB)\n", (unsigned long long)buffer->size, (f64)buffer->size/MB(1));
    printf("--- Read head      : %18llu\n",       (unsigned long long)buffer->read);
    printf("--- Write head     : %18llu\n",       (unsigned long long)buffer->write);

    printf("Initial state of the ring buffer\n");
    print_ring_buffer(buffer);

    printf("Priming buffer: writing up to size - 1\n");
    u8 frame[KB(64)-1];
    ring_buffer_write(buffer, frame, sizeof(frame));
    print_ring_buffer(buffer);

    printf("Priming buffer: advancing read head up to size - 1\n");
    u8 read = 0;
    for (u64 i = 0; i < KB(64); i++)
        ring_buffer_read(buffer, &read, 1);

    printf("Writing actual data across the seam\n");
    u8 ringdata[8];
    for (size_t i = 0; i < 8; ++i)
        ringdata[i] = (u8)i;
    ring_buffer_write(buffer, ringdata, sizeof(ringdata));

    print_ring_buffer(buffer);

    printf("Attempting to loop around and write `0xDD` past the read head ... ");

    u8 write_past_read[KB(64) + 1];
    for (size_t i = 0; i < KB(64) + 1; ++i) write_past_read[i] = 0xDD;

    if(!ring_buffer_write(buffer, write_past_read, sizeof(write_past_read)))
    {
        printf("could not write anything");
    }
    printf("\n");

    print_ring_buffer(buffer);
    printf("Reading actual data across the seam\n");
    u8 ringread[8];
    ring_buffer_read(buffer, ringread, sizeof(ringread));
    printf("read in [ ");
    for (size_t i = 0; i < 8; ++i)
        printf("0x%02X ", ringread[i]);
    printf("]\n");

    print_ring_buffer(buffer);

    printf("Attempting to read past the write head ... ");
    if (!ring_buffer_read(buffer, ringread, sizeof(ringread)))
    {
        printf("Could not read past write head");
    }
    printf("\n");


    ring_buffer_release(buffer);
    arena_release(&arena);


    return 0;
}
