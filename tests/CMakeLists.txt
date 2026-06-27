
add_executable(test_arena
    test_arenas.c
)

target_link_libraries(test_arena
    PRIVATE
        aether::aether
)

# add_test(
#     NAME arena
#     COMMAND test_arena
# )

set(ARENA_TEST_CASES array_count alloc_basic alloc_ex_custom push_alignment push_array_contiguous
    zero_policy pop_to_and_pop temp_basic temp_nested temp_out_of_order_is_safe
    temp_rk4_pattern clear decommit_flag commit_chunked_flag debug_fill_on_clear_flag
    strings overflow_guard release many_small_pushes)

foreach(case ${ARENA_TEST_CASES})
    add_test(NAME arena_${case} COMMAND test_arena ${case})
endforeach()

add_executable(test_strings
    test_strings.c
)

target_link_libraries(test_strings
    PRIVATE
        aether::aether
)

set(STRING_TEST_CASES eq cmp slice trim str_macro views c_str)

foreach(case ${STRING_TEST_CASES})
    add_test(NAME str8_${case} COMMAND test_strings ${case})
endforeach()

add_executable(test_ring
    test_ring.c
)

target_link_libraries(test_ring
    PRIVATE
        aether::aether
)

set(RING_TEST_CASES mirror alloc roundtrip seam peek_no_consume available_advance guards stream counter_wrap)

foreach(case ${RING_TEST_CASES})
    add_test(NAME ring_${case} COMMAND test_ring ${case})
endforeach()

# --- standard-conformance probes (compile is the test; pedantic where supported) ---
enable_language(CXX)   # only needed when building tests; keeps the top-level project C-only

add_executable(verify_c11   verify_c11.c)
add_executable(verify_cxx11 verify_cxx11.cpp)

target_link_libraries(verify_c11   PRIVATE aether::aether)
target_link_libraries(verify_cxx11 PRIVATE aether::aether)

# *_EXTENSIONS OFF forces -std=c11 / -std=c++11 (not the gnu* variants), so extensions are rejected
set_target_properties(verify_c11 PROPERTIES
    C_STANDARD 11   C_STANDARD_REQUIRED ON   C_EXTENSIONS OFF)
set_target_properties(verify_cxx11 PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)

if(MSVC)
    target_compile_options(verify_c11   PRIVATE /permissive-)
    target_compile_options(verify_cxx11 PRIVATE /permissive-)
else()
    target_compile_options(verify_c11   PRIVATE -pedantic-errors)
    target_compile_options(verify_cxx11 PRIVATE -pedantic-errors)
endif()

add_test(NAME verify_c11   COMMAND verify_c11)
add_test(NAME verify_cxx11 COMMAND verify_cxx11)
