from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps
import os, sys
from conan.errors import ConanInvalidConfiguration
import subprocess

class CaveWhereConan(ConanFile):
    name = "CaveWhere"
    license = "GPL"
    author = "Philip Schuchardt vpicaver@gmail.com"
    url = "https://github.com/Cavewhere/Cavewhere"
    description = "Undergound Cave Survey and Mapping Software"
    topics = ("gis")
    version = "2025.2"
    settings = "os", "compiler", "build_type", "arch"
    requires = [
    ("catch2/[>=3.7.1]"),
    ("protobuf/[>=5.27.0]"),

    #We handle survex dependancies here for now, since we're using conan
    ("wxwidgets/[>=3.2.6]"),
    ("glew/[>=2.2.0]"),
    ("libtiff/[>=4.5.1]"),
    ("gdal/[>=3.5.3]"),
    ]

    options = {"system_qt": [True, False]}
    default_options = {"system_qt": True}
    generators = "CMakeDeps", "CMakeToolchain", "VirtualBuildEnv", "VirtualRunEnv"

    def requirements(self):
        self.requires("expat/[>=2.6.2]", override=True)
        self.requires("libpng/[>=1.6.44]", override=True)
        self.requires("proj/[>=9.3.1]")

        # Or add a new requirement!
        if not self.options.system_qt:
            self.requires("qt/[>=6.7.3]")
            self.requires("xkbcommon/[>=1.6.0]", override=True)
            self.requires("sqlite3/[>=3.45.0]") #, override=True) #override seems to use system's sqlite3 and causes issues
        else:
            self.requires("sqlite3/[>=3.44.2]")

    def build_requirements(self):
        self.tool_requires("protobuf/5.27.0")

    def configure(self):
        if not self.options.system_qt:
            self.options["qt"].shared = True
            self.options["qt"].qtshadertools = True
            self.options["qt"].qtdeclarative = True
            self.options["qt"].qtsvg = True
            self.options["qt"].qttools = True
            self.options["qt"].qttranslations = True
            self.options["qt"].qtimageformats = True
            self.options["qt"].with_libjpeg = "libjpeg"

        #Arrow fails on github linux build, disable
        self.options["gdal"].with_arrow = False

        #These options allow on crosscompiling on arm64 -> x86_64 on windows
        #conan doesn't support strawberryperl or msys
        self.options["gdal"].with_curl = False
        self.options["gdal"].with_libiconv = False
        self.options["proj"].with_curl = False

        #This prevents protoc from needing zlib which adds a failing rpath protoc
        self.options["protobuf"].with_zlib=False

        #This prevents xcode build from failing
        self.options["libtiff"].zstd=False

        self._check_perl_locale_po()

    def _check_perl_locale_po(self):
        try:
            subprocess.check_output(["perl", "-MLocale::PO", "-e", "print 'Locale::PO is available'"])
        except subprocess.CalledProcessError:
            raise ConanInvalidConfiguration(
                "Locale::PO Perl module is not installed. Install it with `cpan Locale::PO`."
            )
