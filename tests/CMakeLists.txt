
add_executable(test_arena
    test_arenas.c
)

target_link_libraries(test_arena
    PRIVATE
        aether::aether
)

add_test(
    NAME arena
    COMMAND test_arena
)
