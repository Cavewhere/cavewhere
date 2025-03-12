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
    ("catch2/[>=2.13.10]")
    ]
