from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps
import os, sys
from conan.errors import ConanInvalidConfiguration
import subprocess

class CaveWhereConan(ConanFile):
    name = "CaveWwhereSketch"
    license = "GPL"
    author = "Philip Schuchardt vpicaver@gmail.com"
    url = "https://github.com/Cavewhere/CaveWhereSketch"
    description = "Underground cave drawing application"
    topics = ("gis")
    version = "2025.2"
    settings = "os", "compiler", "build_type", "arch"
    requires = [
    ("catch2/[>=2.13.10]"),
    ("protobuf/5.27.0")
    ]

    generators = "CMakeDeps", "CMakeToolchain", "VirtualBuildEnv", "VirtualRunEnv"

    def build_requirements(self):
        self.tool_requires("protobuf/<host_version>")

    # def generate(self):
    #     tc = CMakeToolchain(self)
    #     # disable the protoc binary build when cross‐compiling for iOS:
    #     tc.cache_variables["protobuf_BUILD_PROTOC_BINARIES"] = False  # :contentReference[oaicite:0]{index=0}
    #     tc.generate()

    #     deps = CMakeDeps(self)
    #     deps.generate()
