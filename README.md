# dawn-bazel-example

Using bazel to link to the Dawn library

## Building

Bazel is required, of course. [Bazelisk](https://github.com/bazelbuild/bazelisk) in particular is recommended since it will download the latest Bazel version for you.

### Unix (Linux/MacOS)

    bazel build //main:hello-world

### Windows

Signal to Bazel the correct path for the VC Tools/LLVM/Clang distribution:

    set BAZEL_LLVM="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\tools\llvm\x64"
    set BAZEL_VC="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC"

Start build:

    bazel build //main:hello-world
