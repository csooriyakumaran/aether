
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
    push_macro_coverage zero_policy pop_to_and_pop temp_basic temp_nested temp_out_of_order_is_safe
    temp_rk4_pattern clear decommit_flag commit_chunked_flag debug_fill_on_clear_flag
    strings overflow_guard release many_small_pushes header_survives_clear first_push_at_header_floor)

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

set(STRING_TEST_CASES eq cmp slice trim str_macro views c_str split
    skip_drop trim_lr eq_nocase prefix_suffix find cut concat join
    case_convert replace parse view_sources)

foreach(case ${STRING_TEST_CASES})
    add_test(NAME str8_${case} COMMAND test_strings ${case})
endforeach()

add_executable(test_files
    test_files.c
)

target_link_libraries(test_files
    PRIVATE
        aether::aether
)

set(FILE_TEST_CASES write_read_roundtrip write_truncates write_failure read_missing map_roundtrip map_missing)

foreach(case ${FILE_TEST_CASES})
    add_test(NAME file_${case} COMMAND test_files ${case})
endforeach()

add_executable(test_ring
    test_ring.c
)

target_link_libraries(test_ring
    PRIVATE
        aether::aether
)

set(RING_TEST_CASES mirror alloc roundtrip seam peek_no_consume available_advance guards stream counter_wrap spsc_stress)

foreach(case ${RING_TEST_CASES})
    add_test(NAME ring_${case} COMMAND test_ring ${case})
endforeach()

add_executable(test_ring_spsc
    test_ring_spsc.c
)

target_link_libraries(test_ring_spsc
    PRIVATE
        aether::aether
)

set(RING_SPSC_TEST_CASES thread_api thread_sleep consumer_hot producer_hot)

foreach(case ${RING_SPSC_TEST_CASES})
    add_test(NAME ring_spsc_${case} COMMAND test_ring_spsc ${case})
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
