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
        "debug.cc",
        "debug.hh",
        "pipe.cc",
        "pipe.hh",
        "protocol.cc",
        "protocol.hh",
        "server.cc",
        "utils.cpp",
    ],
    deps = [
        ":glfw_utils",
        "//deps/libev",
        "@dawn",
        "@dawn//:dawn_wire",
        "@glfw",
    ],
)

cc_binary(
    name = "client",
    srcs = [
        "client.cc",
        "debug.cc",
        "debug.hh",
        "pipe.cc",
        "pipe.hh",
        "protocol.cc",
        "protocol.hh",
    ],
    deps = [
        "//deps/libev",
        "@dawn//:dawn_cpp",
        "@dawn//:dawn_proc",
        "@dawn//:dawn_utils",
        "@dawn//:dawn_wire",
    ],
)

objc_library(
    name = "glfw_utils",
    srcs = ["utils_metal.mm"],
    copts = [
        "-std=c++20",
        "-fno-objc-arc",
    ],
    defines = [
        # From dawn/src/dawn/common/BUILD.gn:internal_config
        "DAWN_ENABLE_BACKEND_METAL",
    ],
    sdk_frameworks = [
        "IOKit",
        "IOSurface",
        "Metal",
        "QuartzCore",
    ],
    target_compatible_with = select({
        "@platforms//os:osx": [],
        "//conditions:default": ["@platforms//:incompatible"],
    }),
    visibility = ["//visibility:private"],  # only used by :dawn
    deps = [
        "@dawn",
        "@glfw",
    ],
)
