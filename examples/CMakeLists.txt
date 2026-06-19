add_executable(aether-example
    example.c
)

target_link_libraries(aether-example
    PRIVATE
        aether::aether
)

