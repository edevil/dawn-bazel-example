load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "bazel_skylib",
    sha256 = "c6966ec828da198c5d9adbaa94c05e3a1c7f21bd012a0b29ba8ddbccb2c93b0d",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.1.1/bazel-skylib-1.1.1.tar.gz",
        "https://github.com/bazelbuild/bazel-skylib/releases/download/1.1.1/bazel-skylib-1.1.1.tar.gz",
    ],
)

load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")

bazel_skylib_workspace()

#######################################################################################
# Python
#######################################################################################

# https://github.com/bazelbuild/rules_python
http_archive(
    name = "rules_python",
    sha256 = "84aec9e21cc56fbc7f1335035a71c850d1b9b5cc6ff497306f84cced9a769841",
    strip_prefix = "rules_python-0.23.1",
    url = "https://github.com/bazelbuild/rules_python/releases/download/0.23.1/rules_python-0.23.1.tar.gz",
)

# This sets up a hermetic python3, rather than depending on what is installed.
load("@rules_python//python:repositories.bzl", "python_register_toolchains")

python_register_toolchains(
    name = "python3_11",
    # https://github.com/bazelbuild/rules_python/blob/main/python/versions.bzl
    python_version = "3.11",
)

load("@python3_11//:defs.bzl", "interpreter")
load("@rules_python//python:pip.bzl", "pip_parse")

pip_parse(
    name = "py_deps",
    python_interpreter_target = interpreter,
    requirements = "//:requirements.txt",
)

load("@py_deps//:requirements.bzl", "install_deps")

install_deps()

#######################################################################################
# /Python
#######################################################################################

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

git_repository(
    name = "dawn",
    build_file = "@//:BUILD.dawn",
    #branch = "chromium/5793",
    commit = "fd61f6244fb00ea42390f5a77267a4c195d90a06",
    remote = "https://dawn.googlesource.com/dawn.git",
)

git_repository(
    name = "abseil_cpp",
    commit = "66406fdf1553de6ad672576167e7cc7ca2d764cb",
    remote = "https://github.com/abseil/abseil-cpp.git",
)

git_repository(
    name = "vulkan_tools",
    build_file = "@//:BUILD.vulkan_tools",
    commit = "ca8bb4ee3cc9afdeca4b49c5ef758bad7cce2c72",
    remote = "https://github.com/KhronosGroup/Vulkan-Tools.git",
)

git_repository(
    name = "vulkan_headers",
    build_file = "@//:BUILD.vulkan_headers",
    commit = "c1a8560c5cf5e7bd6dbc71fe69b1a317411c36b8",
    remote = "https://github.com/KhronosGroup/Vulkan-Headers.git",
)

git_repository(
    name = "spirv_tools",
    commit = "a63ac9f73d29cd27cdb6e3388d98d1d934e512bb",
    remote = "https://github.com/KhronosGroup/SPIRV-Tools.git",
)

git_repository(
    name = "spirv_headers",
    commit = "6e09e44cd88a5297433411b2ee52f4cf9f50fa90",
    remote = "https://github.com/KhronosGroup/SPIRV-Headers.git",
)

# Hedron's Compile Commands Extractor for Bazel
# https://github.com/hedronvision/bazel-compile-commands-extractor
http_archive(
    name = "hedron_compile_commands",
    strip_prefix = "bazel-compile-commands-extractor-3dddf205a1f5cde20faf2444c1757abe0564ff4c",

    # Replace the commit hash in both places (below) with the latest, rather than using the stale one here.
    # Even better, set up Renovate and let it do the work for you (see "Suggestion: Updates" in the README).
    url = "https://github.com/hedronvision/bazel-compile-commands-extractor/archive/3dddf205a1f5cde20faf2444c1757abe0564ff4c.tar.gz",
    # When you first run this tool, it'll recommend a sha256 hash to put here with a message like: "DEBUG: Rule 'hedron_compile_commands' indicated that a canonical reproducible form can be obtained by modifying arguments sha256 = ..."
)

load("@hedron_compile_commands//:workspace_setup.bzl", "hedron_compile_commands_setup")

hedron_compile_commands_setup()
