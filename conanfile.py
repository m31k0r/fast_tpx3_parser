from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain


class MyProject(ConanFile):
    name = "my-project"
    version = "1.0"

    settings = "os", "arch", "compiler", "build_type"

    requires = (
        "gtest/1.17.0",
        "benchmark/1.9.4",
        "flux/0.4.0",
    )

    generators = (
        CMakeDeps,
        CMakeToolchain,
    )
