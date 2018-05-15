# -*- python -*-

env = Environment(
    CCFLAGS=[
        "-Wall",
        "-Wextra",
        "-pedantic",
        "-Werror",
        "-std=c++17",
        "-g",
        "-fsanitize=address",
    ],
    LINKFLAGS=[
        "-fsanitize=address",
    ],
)

env.Program(
    "cadit",
    source=[
        "main.cpp",
        "tokenize.cpp",
    ],
)
