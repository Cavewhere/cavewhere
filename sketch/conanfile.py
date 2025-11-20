from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps
import os, sys
from conan.errors import ConanInvalidConfiguration
import subprocess

class CaveWhereConan(ConanFile):
    name = "CaveWhereSketch"
    license = "GPL"
    author = "Philip Schuchardt vpicaver@gmail.com"
    url = "https://github.com/Cavewhere/CaveWhereSketch"
    description = "Underground cave drawing application"
    topics = ("gis")
    version = "2025.2"
    settings = "os", "compiler", "build_type", "arch"
    requires = [
    ("catch2/[>=2.13.10]"),
    ("protobuf/6.30.1"),


    #For QQuickGit
    ("libgit2/1.9.0"),
    #("openssl/3.5.0"),
    ("libssh2/[>=1.11]")
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

    def requirements(self):
        # if self.settings.os == "Android":
        #     self.requires("openssl/3.5.0@test/demo", override=True)
        # else:
            self.requires("openssl/3.5.0")

    def configure(self):
        self.options["openssl"].shared = True

        if self.settings.os == "Android":
            self.options["openssl"].shared = False
        #     self.options["openssl"].no_asm = True #Windows building android, probably can comment out for other platforms

        if self.settings.os == "iOS":
            self.options["openssl"].shared = False
            self.options["sqlite3"].build_executable = False
            self.options["libgit2"].with_regex = "builtin"
