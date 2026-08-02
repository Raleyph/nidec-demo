idf_component_register(
    SRCS
        "main.c"
        "motor.c"
        "console.c"

    INCLUDE_DIRS
        "."

    REQUIRES
        console
)