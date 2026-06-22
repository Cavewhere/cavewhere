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
    ("minizip-ng/[>=4.0.7]"),
    ("laslib/[>=2.0.2]")
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
        # PROJ is needed by survex (cavernlib/survex_core) on all platforms
        self.requires("proj/[=9.3.1]")

        if not self.options.mobile:
            # libtiff is pulled in by PROJ (with_tiff); pin it here for the desktop build.
            self.requires("libtiff/[>=4.5.1]")

        if self.settings.os == "Linux":
            self.requires("xorg-proto/[=2024.1]", override=True)
            # libgcrypt/1.12.2 requires libgpg-error>=1.56, but Conan resolves
            # libgpg-error to an older revision and the build fails. Pin to the
            # previous libgcrypt release, which libsecret (via wxwidgets) accepts.
            self.requires("libgcrypt/1.10.3", override=True)

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
            self.requires("sqlite3/[>=3.48.0]", override=True)

    def build_requirements(self):
        self.tool_requires("protobuf/<host_version>")

    def configure(self):
        #This prevents protoc from needing zlib which adds a failing rpath protoc
        #Disable for windows
        self.options["protobuf"].with_zlib=True
        is_mobile = self.options.mobile or self.settings.os in ["iOS", "Android"]
        is_windows = self.settings.os == "Windows"
        # Windows desktop: shared protobuf requires shared abseil.
        if not is_mobile and is_windows:
            self.options["protobuf"].shared = True
            self.options["abseil"].shared = True
        elif is_mobile:
            # Mobile (iOS/Android): everything must be static.
            self.options["protobuf"].shared = False
            self.options["abseil"].shared = False
        else:
            # macOS/Linux desktop: shared protobuf, static abseil.
            self.options["protobuf"].shared = True
            self.options["abseil"].shared = False

        self.options["openssl"].shared = True

        if self.settings.os == "Android":
            # On Android, Conan-built OpenSSL is only used for the static libs
            # that libssh2/libgit2/qtkeychain link into the app binary. Qt's
            # TLS plugin dlopens OpenSSL separately at runtime via KDAB's
            # prebuilt libssl_3.so/libcrypto_3.so, wired up through
            # QT_ANDROID_EXTRA_LIBS in the top-level CMakeLists.txt.
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

        self.options["proj"].with_curl = False

        if self.options.mobile:
            # Disable TIFF to avoid libjpeg version conflict: proj → libtiff → libjpeg 9e
            # clashes with Qt's libjpeg-turbo (version 80) in the static iOS link.
            self.options["proj"].with_tiff = False
        else:
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
