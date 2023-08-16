load("@rules_foreign_cc//foreign_cc:configure.bzl", "configure_make")

package(default_visibility = ["//visibility:public"])

configure_make(
    name = "libev",
    configure_options = [
        "--disable-dependency-tracking",
        "--enable-static",
        "--disable-shared",
    ],
    # Without this for some reason Bazel tries to send
    # ar commands to libtool!
    env = {
        "AR": "",
    },
    lib_source = "@libev//:all",
    # Bazel likes to use the shared libraries for tests
    out_static_libs = ["libev.a"],
)
