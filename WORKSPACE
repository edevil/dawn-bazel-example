load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

#######################################################################################
# Python
#######################################################################################

# https://github.com/bazelbuild/rules_python
http_archive(
    name = "rules_python",
    sha256 = "5fa3c738d33acca3b97622a13a741129f67ef43f5fdfcec63b29374cc0574c29",
    strip_prefix = "rules_python-0.9.0",
    urls = [
        "https://storage.googleapis.com/skia-world-readable/bazel/5fa3c738d33acca3b97622a13a741129f67ef43f5fdfcec63b29374cc0574c29.tar.gz",
        "https://github.com/bazelbuild/rules_python/archive/refs/tags/0.9.0.tar.gz",
    ],
)

# This sets up a hermetic python3, rather than depending on what is installed.
load("@rules_python//python:repositories.bzl", "python_register_toolchains")

python_register_toolchains(
    name = "python3_9",
    # https://github.com/bazelbuild/rules_python/blob/main/python/versions.bzl
    python_version = "3.9",
)

load("@python3_9//:defs.bzl", "interpreter")
load("@rules_python//python:pip.bzl", "pip_install")

pip_install(
    name = "py_deps",
    python_interpreter_target = interpreter,
    requirements = "//:requirements.txt",
)

#######################################################################################
# /Python
#######################################################################################

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository", "new_git_repository")

new_git_repository(
    name = "dawn",
    build_file = "@//bazel/external/dawn:BUILD.bazel",
    commit = "0d5e76a2427f1c629a0d709ee0833da43bf79e84",
    remote = "https://dawn.googlesource.com/dawn.git",
)

git_repository(
    name = "abseil_cpp",
    commit = "cb436cf0142b4cbe47aae94223443df7f82e2920",
    remote = "https://skia.googlesource.com/external/github.com/abseil/abseil-cpp.git",
)

new_git_repository(
    name = "vulkan_tools",
    build_file = "@//vulkan_tools:BUILD.bazel",
    commit = "ca8bb4ee3cc9afdeca4b49c5ef758bad7cce2c72",
    remote = "https://chromium.googlesource.com/external/github.com/KhronosGroup/Vulkan-Tools",
)

new_git_repository(
    name = "vulkan_headers",
    build_file = "@//vulkan_headers:BUILD.bazel",
    commit = "c1a8560c5cf5e7bd6dbc71fe69b1a317411c36b8",
    remote = "https://chromium.googlesource.com/external/github.com/KhronosGroup/Vulkan-Headers",
)

git_repository(
    name = "spirv_tools",
    commit = "a63ac9f73d29cd27cdb6e3388d98d1d934e512bb",
    remote = "https://skia.googlesource.com/external/github.com/KhronosGroup/SPIRV-Tools.git",
)

git_repository(
    name = "spirv_headers",
    commit = "6e09e44cd88a5297433411b2ee52f4cf9f50fa90",
    remote = "https://skia.googlesource.com/external/github.com/KhronosGroup/SPIRV-Headers.git",
)

#######################################################################################
# Foreign rules for CMake, maybe not needed
#######################################################################################

http_archive(
    name = "rules_foreign_cc",
    sha256 = "2a4d07cd64b0719b39a7c12218a3e507672b82a97b98c6a89d38565894cf7c51",
    strip_prefix = "rules_foreign_cc-0.9.0",
    url = "https://github.com/bazelbuild/rules_foreign_cc/archive/refs/tags/0.9.0.tar.gz",
)

load("@rules_foreign_cc//foreign_cc:repositories.bzl", "rules_foreign_cc_dependencies")

# This sets up some common toolchains for building targets. For more details, please see
# https://bazelbuild.github.io/rules_foreign_cc/0.9.0/flatten.html#rules_foreign_cc_dependencies
rules_foreign_cc_dependencies()
