add_executable(aether-example
    example.c
)

target_link_libraries(aether-example
    PRIVATE
        aether::aether
)

add_executable(aether-example-timer
    timer.c
)

target_link_libraries(aether-example-timer
    PRIVATE
        aether::aether
)

