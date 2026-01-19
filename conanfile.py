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
    ("protobuf/[=6.32.1]"),
    ("libgit2/1.9.1"),
    ("libssh2/[>=1.11]"),
    ("openssl/3.5.0"),
    ("xxhash/[>=0.8.3]"),
    ("tinygltf/[>=2.9.0]"),
    ("minizip-ng/[>=4.0.7]")
    ]

    options = {
    "system_qt": [True, False],
    "mobile": [True, False]
    }

    default_options = {
    "system_qt": True,
    "mobile": False
    }

    generators = "CMakeDeps", "CMakeToolchain", "VirtualBuildEnv", "VirtualRunEnv"

    def requirements(self):
        if not self.options.mobile:
            #We handle survex dependancies here for now, since we're using conan
            self.requires("wxwidgets/[>=3.2.6]")
            self.requires("glew/[>=2.2.0]")
            self.requires("libtiff/[>=4.5.1]")
            self.requires("gdal/[>=3.5.3]")
            self.requires("expat/[>=2.6.2]", override=True)
            self.requires("libpng/[>=1.6.44]", override=True)
            self.requires("proj/[>=9.3.1]")

        #Fixes a build issue on macos
        # if self.settings.os == "Macos":
        #     # Fixes a build issue on macOS
        #     self.requires("abseil/[>=20250127.0]")
        # else:
        self.requires("abseil/[=20250814.0]", override=True)

        # Or add a new requirement!
        if not self.options.system_qt:
            self.requires("qt/6.10.1")
            self.requires("xkbcommon/[>=1.6.0]", override=True)
            self.requires("sqlite3/[>=3.45.0]") #, override=True) #override seems to use system's sqlite3 and causes issues
        else:
            self.requires("sqlite3/[>=3.44.2]")

    def build_requirements(self):
        self.tool_requires("protobuf/<host_version>")

    def configure(self):
        #This prevents protoc from needing zlib which adds a failing rpath protoc
        #Disable for windows
        self.options["protobuf"].with_zlib=True
        is_mobile = self.options.mobile or self.settings.os in ["iOS", "Android"]
        # Desktop uses shared protobuf to avoid duplicate static runtimes in plugin-heavy builds.
        self.options["protobuf"].shared = not is_mobile
        # Protobuf shared requires abseil shared on desktop (especially on Windows).
        self.options["abseil"].shared = not is_mobile

        self.options["openssl"].shared = True

        if self.settings.os == "Android":
            self.options["openssl"].shared = False
        #     self.options["openssl"].no_asm = True #Windows building android, probably can comment out for other platforms

        if self.settings.os == "iOS":
            self.options["openssl"].shared = False
            self.options["sqlite3"].build_executable = False
            self.options["libgit2"].with_regex = "builtin"

        if not self.options.system_qt:
            self.options["qt"].shared = True
            self.options["qt"].qtshadertools = True
            self.options["qt"].qtdeclarative = True
            self.options["qt"].qtsvg = True
            self.options["qt"].qttools = True
            self.options["qt"].qttranslations = True
            self.options["qt"].qtimageformats = True
            self.options["qt"].with_libjpeg = "libjpeg"
            self.options["qt"].with_dbus = True

        if not self.options.mobile:
            #Arrow fails on github linux build, disable
            self.options["gdal"].with_arrow = False

            #These options allow on crosscompiling on arm64 -> x86_64 on windows
            #conan doesn't support strawberryperl or msys
            self.options["gdal"].with_curl = False
            self.options["gdal"].with_libiconv = False
            self.options["proj"].with_curl = False


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
