load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
    name = "hello-world",
    srcs = ["hello-world.cc"],
    deps = [
        "@dawn",
    ],
)

cc_binary(
    name = "server",
    srcs = [
        "common.cc",
        "common.hh",
        "debug.cc",
        "debug.hh",
        "pipe.cc",
        "pipe.hh",
        "protocol.cc",
        "protocol.hh",
        "server.cc",
    ],
    defines = ["DEBUG"],
    deps = [
        "//deps/libev",
        "@dawn",
        "@dawn//:dawn_wire",
    ],
)

cc_binary(
    name = "client",
    srcs = [
        "client.cc",
        "common.cc",
        "common.hh",
        "debug.cc",
        "debug.hh",
        "pipe.cc",
        "pipe.hh",
        "protocol.cc",
        "protocol.hh",
    ],
    defines = ["DEBUG"],
    deps = [
        "//deps/libev",
        "@dawn//:dawn_cpp",
        "@dawn//:dawn_proc",
        "@dawn//:dawn_wire",
    ],
)
