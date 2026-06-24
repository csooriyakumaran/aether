
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

set(ARENA_TEST_CASES alloc_basic alloc_ex_custom push_alignment push_array_contiguous
    zero_policy pop_to_and_pop clear decommit_flag commit_chunked_flag
    debug_fill_on_clear_flag strings overflow_guard release many_small_pushes)

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

set(STRING_TEST_CASES eq cmp slice)

foreach(case ${STRING_TEST_CASES})
    add_test(NAME str8_${case} COMMAND test_strings ${case})
endforeach()
