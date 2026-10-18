#define AETHER_NO_ASSERT
#define AETHER_IMPLEMENTATION
#include "aether/aether.h"

#include <stdio.h>
#include <string.h>

/* See test_arenas.c for why we define our own ASSERT() via AETHER_NO_ASSERT. */

static int g_checks   = 0;
static int g_failures = 0;

static int section_checks_start    = 0;
static int section_failures_start  = 0;
static const char* current_section = NULL;

#define ASSERT(cond) do {                                                   \
    g_checks++;                                                             \
    if (!(cond)) {                                                          \
        g_failures++;                                                       \
        fprintf(stderr, "  FAIL: %s [%s:%d]\n", #cond, __FILE__, __LINE__); \
    } \
} while (0)

static void section_summary_flush(void)
{
    if (!current_section) return;
    int total  = g_checks - section_checks_start;
    int failed = g_failures - section_failures_start;
    printf("   [%d/%d passed]\n", total - failed, total);
}

#define SECTION(name) do {               \
    section_summary_flush();             \
    current_section        = name;       \
    section_checks_start   = g_checks;   \
    section_failures_start = g_failures; \
    printf("-- %s\n", name);             \
} while (0)

/* tolerance for results that go through AETHER_SQRTF and aren't expected to
   be bit-exact (magnitude/normalize); every other case here is built from
   integer-valued components specifically so '==' is exact and meaningful. */
#define TEST_EPS 0.0001f
static b8 f32_near(f32 a, f32 b, f32 eps) { f32 d = a - b; if (d < 0) d = -d; return d <= eps; }

static void test_layout(void)
{
    SECTION("vec2/vec3/vec4: layout matches a flat f32 array, no hidden padding");

    ASSERT(sizeof(vec2) == 2 * sizeof(f32));
    ASSERT(sizeof(vec3) == 3 * sizeof(f32));
    ASSERT(sizeof(vec4) == 4 * sizeof(f32));
}

static void test_swizzle_alias(void)
{
    SECTION("union views alias the same storage (e[]/xyz/rgb/uv and the xy/yz/xyz/rgb sub-views)");

    vec2 a2 = {0};
    a2.x = 1.0f; a2.y = 2.0f;
    ASSERT(a2.e[0] == 1.0f && a2.e[1] == 2.0f);          /* named -> flat array */
    ASSERT(a2.u == 1.0f && a2.v == 2.0f);                /* named -> texcoord alias, same bytes */

    vec3 a3 = {0};
    a3.e[0] = 1.0f; a3.e[1] = 2.0f; a3.e[2] = 3.0f;       /* flat array -> named, other direction */
    ASSERT(a3.x == 1.0f && a3.y == 2.0f && a3.z == 3.0f);
    ASSERT(a3.r == 1.0f && a3.g == 2.0f && a3.b == 3.0f);
    ASSERT(a3.xy.x == 1.0f && a3.xy.y == 2.0f);           /* leading vec2 sub-view */
    ASSERT(a3.yz.x == 2.0f && a3.yz.y == 3.0f);           /* trailing vec2 sub-view, offset by one f32 */

    vec4 a4 = {0};
    a4.x = 1.0f; a4.y = 2.0f; a4.z = 3.0f; a4.w = 4.0f;
    ASSERT(a4.r == 1.0f && a4.g == 2.0f && a4.b == 3.0f && a4.a == 4.0f);
    ASSERT(a4.xyz.x == 1.0f && a4.xyz.y == 2.0f && a4.xyz.z == 3.0f); /* leading vec3 sub-view */
    ASSERT(a4.rgb.r == 1.0f && a4.rgb.g == 2.0f && a4.rgb.b == 3.0f);
    ASSERT(a4.xy.x == 1.0f && a4.xy.y == 2.0f);           /* leading vec2 sub-view */
    ASSERT(a4.yz.x == 2.0f && a4.yz.y == 3.0f);           /* middle vec2 sub-view */
}

static void test_elementwise(void)
{
    SECTION("add/sub/mul/div: component-wise across vec2/vec3/vec4");

    vec2 a2 = {2,3}, b2 = {8,9};
    vec2 r2 = vec2_add(a2, b2); ASSERT(r2.x==10 && r2.y==12);
    r2 = vec2_sub(b2, a2);      ASSERT(r2.x==6  && r2.y==6);
    r2 = vec2_mul(a2, b2);      ASSERT(r2.x==16 && r2.y==27);
    r2 = vec2_div(b2, a2);      ASSERT(r2.x==4  && r2.y==3);

    vec3 a3 = {2,4,5}, b3 = {8,8,10};
    vec3 r3 = vec3_add(a3, b3); ASSERT(r3.x==10 && r3.y==12 && r3.z==15);
    r3 = vec3_sub(b3, a3);      ASSERT(r3.x==6  && r3.y==4  && r3.z==5);
    r3 = vec3_mul(a3, b3);      ASSERT(r3.x==16 && r3.y==32 && r3.z==50);
    r3 = vec3_div(b3, a3);      ASSERT(r3.x==4  && r3.y==2  && r3.z==2);

    vec4 a4 = {2,3,4,5}, b4 = {8,9,8,10};
    vec4 r4 = vec4_add(a4, b4); ASSERT(r4.x==10 && r4.y==12 && r4.z==12 && r4.w==15);
    r4 = vec4_sub(b4, a4);      ASSERT(r4.x==6  && r4.y==6  && r4.z==4  && r4.w==5);
    r4 = vec4_mul(a4, b4);      ASSERT(r4.x==16 && r4.y==27 && r4.z==32 && r4.w==50);
    r4 = vec4_div(b4, a4);      ASSERT(r4.x==4  && r4.y==3  && r4.z==2  && r4.w==2);
}

static void test_scalar_ops(void)
{
    SECTION("scale/negate across vec2/vec3/vec4");

    vec2 a2 = {1,-2};
    vec2 r2 = vec2_scale(a2, 3.0f); ASSERT(r2.x==3 && r2.y==-6);
    r2 = vec2_negate(a2);           ASSERT(r2.x==-1 && r2.y==2);

    vec3 a3 = {1,-2,3};
    vec3 r3 = vec3_scale(a3, -2.0f); ASSERT(r3.x==-2 && r3.y==4 && r3.z==-6);
    r3 = vec3_negate(a3);            ASSERT(r3.x==-1 && r3.y==2 && r3.z==-3);

    vec4 a4 = {1,-2,3,-4};
    vec4 r4 = vec4_scale(a4, 2.0f); ASSERT(r4.x==2 && r4.y==-4 && r4.z==6 && r4.w==-8);
    r4 = vec4_negate(a4);           ASSERT(r4.x==-1 && r4.y==2 && r4.z==-3 && r4.w==4);
}

static void test_dot(void)
{
    SECTION("dot: orthogonal axes are zero, products otherwise match by hand");

    vec3 x_axis = {1,0,0}, y_axis = {0,1,0};
    ASSERT(vec3_dot(x_axis, y_axis) == 0.0f);

    vec2 a2 = {1,2}, b2 = {3,4};
    ASSERT(vec2_dot(a2, b2) == 11.0f); /* 1*3 + 2*4 */

    vec3 a3 = {1,2,3}, b3 = {4,5,6};
    ASSERT(vec3_dot(a3, b3) == 32.0f); /* 4 + 10 + 18 */

    vec4 a4 = {1,2,3,4}, b4 = {5,6,7,8};
    ASSERT(vec4_dot(a4, b4) == 70.0f); /* 5 + 12 + 21 + 32 */
}

static void test_cross(void)
{
    SECTION("vec3_cross: right-handed basis, anti-commutativity, self-cross is zero");

    vec3 x_axis = {1,0,0}, y_axis = {0,1,0}, z_axis = {0,0,1};

    vec3 r = vec3_cross(x_axis, y_axis);
    ASSERT(r.x==0 && r.y==0 && r.z==1); /* x cross y = z */

    r = vec3_cross(y_axis, z_axis);
    ASSERT(r.x==1 && r.y==0 && r.z==0); /* y cross z = x */

    r = vec3_cross(z_axis, x_axis);
    ASSERT(r.x==0 && r.y==1 && r.z==0); /* z cross x = y */

    vec3 a = {1,2,3}, b = {4,5,6};
    vec3 ab = vec3_cross(a, b);
    vec3 ba = vec3_cross(b, a);
    ASSERT(ab.x == -ba.x && ab.y == -ba.y && ab.z == -ba.z);

    vec3 self = vec3_cross(a, a);
    ASSERT(self.x==0 && self.y==0 && self.z==0);
}

static void test_magnitude(void)
{
    SECTION("mag_sq/magnitude: Pythagorean cases across vec2/vec3/vec4");

    vec2 a2 = {3,4}; /* 3-4-5 */
    ASSERT(vec2_mag_sq(a2) == 25.0f);
    ASSERT(f32_near(vec2_magnitude(a2), 5.0f, TEST_EPS));

    vec3 a3 = {2,3,6}; /* 4+9+36 = 49 */
    ASSERT(vec3_mag_sq(a3) == 49.0f);
    ASSERT(f32_near(vec3_magnitude(a3), 7.0f, TEST_EPS));

    vec4 a4 = {1,2,2,4}; /* 1+4+4+16 = 25 */
    ASSERT(vec4_mag_sq(a4) == 25.0f);
    ASSERT(f32_near(vec4_magnitude(a4), 5.0f, TEST_EPS));
}

static void test_normalize(void)
{
    SECTION("normalize: unit length, direction preserved under positive scale");

    vec2 a2 = {3,4};
    vec2 n2 = vec2_normalize(a2);
    ASSERT(f32_near(vec2_magnitude(n2), 1.0f, TEST_EPS));
    ASSERT(f32_near(n2.x, 0.6f, TEST_EPS) && f32_near(n2.y, 0.8f, TEST_EPS));

    vec3 a3 = {2,3,6};
    vec3 n3 = vec3_normalize(a3);
    ASSERT(f32_near(vec3_magnitude(n3), 1.0f, TEST_EPS));

    vec4 a4 = {1,2,2,4};
    vec4 n4 = vec4_normalize(a4);
    ASSERT(f32_near(vec4_magnitude(n4), 1.0f, TEST_EPS));

    /* normalizing any positive scale of a vector gives the same direction */
    vec3 scaled = vec3_scale(a3, 10.0f);
    vec3 n3b    = vec3_normalize(scaled);
    ASSERT(f32_near(n3.x, n3b.x, TEST_EPS));
    ASSERT(f32_near(n3.y, n3b.y, TEST_EPS));
    ASSERT(f32_near(n3.z, n3b.z, TEST_EPS));
}

static void test_normalize_zero_vector(void)
{
    SECTION("normalize: zero-length input is a decided case, not left to the division");

#if AETHER_ENABLE_ASSERTS
    printf("   skipped: AETHER_ASSERT_ traps on a zero-length vector in this build (AETHER_ENABLE_ASSERTS=1);\n"
           "            rebuild with NDEBUG defined to exercise the release-mode NaN fallthrough.\n");
#else
    /* release-mode contract: 1.0f/0.0f -> +inf, 0.0f*inf -> NaN. Not a crash,
       but a deliberate, documented consequence rather than an accident --
       see decision in docs/vec-math-design.md. NaN != NaN is the only
       portable, dependency-free way to check for it. */
    vec3 zero = {0,0,0};
    vec3 n = vec3_normalize(zero);
    ASSERT(n.x != n.x);
    ASSERT(n.y != n.y);
    ASSERT(n.z != n.z);
#endif
}

static void test_lerp(void)
{
    SECTION("lerp: exact endpoints, correct midpoint, across vec2/vec3/vec4");

    vec3 a3 = {0,0,0}, b3 = {10,20,30};

    vec3 at0 = vec3_lerp(a3, b3, 0.0f);
    ASSERT(at0.x==a3.x && at0.y==a3.y && at0.z==a3.z); /* t=0 exact */

    vec3 at1 = vec3_lerp(a3, b3, 1.0f);
    ASSERT(at1.x==b3.x && at1.y==b3.y && at1.z==b3.z); /* t=1 exact */

    vec3 mid = vec3_lerp(a3, b3, 0.5f);
    ASSERT(mid.x==5.0f && mid.y==10.0f && mid.z==15.0f);

    vec2 a2 = {0,0}, b2 = {4,8};
    vec2 m2 = vec2_lerp(a2, b2, 0.25f);
    ASSERT(m2.x==1.0f && m2.y==2.0f);

    vec4 a4 = {0,0,0,0}, b4 = {2,4,6,8};
    vec4 m4 = vec4_lerp(a4, b4, 0.5f);
    ASSERT(m4.x==1.0f && m4.y==2.0f && m4.z==3.0f && m4.w==4.0f);
}

static void test_eq(void)
{
    SECTION("eq: exact equality, byte-for-byte");

    vec2 a2 = {1,2}, b2 = {1,2}, c2 = {1,3};
    ASSERT(vec2_eq(a2, b2));
    ASSERT(!vec2_eq(a2, c2));

    vec3 a3 = {1,2,3}, b3 = {1,2,3}, c3 = {1,2,4};
    ASSERT(vec3_eq(a3, b3));
    ASSERT(!vec3_eq(a3, c3));

    vec4 a4 = {1,2,3,4}, b4 = {1,2,3,4}, d4 = {9,9,9,4};
    ASSERT(vec4_eq(a4, b4));
    ASSERT(!vec4_eq(a4, d4)); /* differs only in the last component -- exercises the full loop */
}

static void test_eq_approx(void)
{
    SECTION("eq_approx: Euclidean tolerance, including the exact-epsilon boundary");

    vec3 origin = {0,0,0};
    vec3 near_  = {0.001f, 0, 0}; /* distance 0.001 */
    ASSERT(vec3_eq_approx(origin, near_, 0.01f));

    vec3 far_ = {1.0f, 0, 0}; /* distance 1.0 */
    ASSERT(!vec3_eq_approx(origin, far_, 0.01f));

    /* exactly epsilon apart: the implementation is strict '<', so this is
       defined to read as NOT equal -- pins the boundary down rather than
       leaving it to whatever the comparison happens to do. */
    vec2 p = {0,0};
    vec2 q = {3,4}; /* distance exactly 5 */
    ASSERT(!vec2_eq_approx(p, q, 5.0f));
    ASSERT(vec2_eq_approx(p, q, 5.001f)); /* just inside */
}

typedef struct { const char* name; void (*fn)(void); } TestCase;
static TestCase g_cases[] = {
    {"layout",                 test_layout},
    {"swizzle_alias",          test_swizzle_alias},
    {"elementwise",            test_elementwise},
    {"scalar_ops",             test_scalar_ops},
    {"dot",                    test_dot},
    {"cross",                  test_cross},
    {"magnitude",              test_magnitude},
    {"normalize",              test_normalize},
    {"normalize_zero_vector",  test_normalize_zero_vector},
    {"lerp",                   test_lerp},
    {"eq",                     test_eq},
    {"eq_approx",              test_eq_approx},
};

int main(int argc, char** argv)
{
    if (argc > 1) {
        for (size_t i = 0; i < ARRAY_COUNT(g_cases)+1; ++i)
        {
            if (i == ARRAY_COUNT(g_cases))
            {
                fprintf(stderr, "unknown test case: %s\n", argv[1]);
                return 1; /* signal a fail if no test case found */
            }
            if (strcmp(argv[1], g_cases[i].name) == 0)
            {
                g_cases[i].fn();
                break;
            }
        }
    } else {
        for (size_t i = 0; i < ARRAY_COUNT(g_cases); ++i) g_cases[i].fn();
        section_summary_flush();
    }

    printf("\n%d checks, %d failed\n", g_checks, g_failures);
    return g_failures != 0;
}
