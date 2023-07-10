# dawn-bazel-example
Using bazel to link to the Dawn library


# Building

Bazel is required, of course. [Bazelisk](https://github.com/bazelbuild/bazelisk) in particular is recommended since it will download the latest Bazel version for you.
## Unix (Linux/MacOS)

    bazel build --config=unix //main:hello-world

## Windows

    bazel build --config=windows //main:hello-world
