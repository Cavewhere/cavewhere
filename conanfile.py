from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps
import os, sys

class CaveWhereConan(ConanFile):
    name = "CaveWhere"
    license = "GPL"
    author = "Philip Schuchardt vpicaver@gmail.com"
    url = "https://github.com/Cavewhere/Cavewhere"
    description = "Undergound Cave Survey and Mapping Software"
    topics = ("gis")
    version = "1.0"
    settings = "os", "compiler", "build_type", "arch"
    requires = [
    ("catch2/[>=2.13.10]"),
    ("protobuf/[>=3.12.4]"),
#    ("survex/1.2.44@cave-software/dev"),
#    ("dewalls/7e97092a144f153cb9ed7d318808208e9b35c74f@cave-software/dev"),
#   ("sqlite3/[>=3.42.0]"}),
    ("libsquish/[>=1.15]"),

    #We handle survex dependancies here for now, since we're using conan
    ("wxwidgets/[>=3.2.5]"),
    ("glew/[>=2.2.0]"),
    ("proj/9.3.1"),
    # ("zlib/[>=1.2.13]"),
    ("libtiff/[>=4.5.1]"),
    ("gdal/3.7.3"),
    ]

    options = {"system_qt": [True, False]}
    default_options = {"system_qt": True}
    generators = "CMakeDeps", "CMakeToolchain", "VirtualBuildEnv", "VirtualRunEnv"

    def requirements(self):
        self.requires("expat/2.6.2", override=True)
        self.requires("sqlite3/3.46.1", override=True)
        # self.requires("proj/9.5.0", override=True)
        self.requires("libpng/1.6.44", override=True)

        # Or add a new requirement!
        # if not self.options.system_qt:
            # self.requires("qt/6.7.3")
            # self.requires("xkbcommon/1.6.0", override=True)
        #    self.requires("libpng/1.6.40"),
        #    self.requires("libjpeg/9e"),

#    def set_version(self):
#        git = tools.Git()
#        refStr = git.run("ls-remote -h https://github.com/Cavewhere/dewalls.git")
#        refs = refStr.split("\n")

#        sha = "unknown-sha"
#        for ref in refs:
#            print("ref:{0}".format(ref))
#            if "refs/heads/master" in ref:
#                sha = ref.split()[0]

#        self.version = "{0}".format(sha)

#    def configure(self):
        # if not self.options.system_qt:
        #     self.options["qt"].shared = True
        #     self.options["qt"].qtshadertools = True
        #     self.options["qt"].qtdeclarative = True
        #     self.options["qt"].qtsvg = True
        #     self.options["qt"].qttools = True
        #     self.options["qt"].qttranslations = True
        #     self.options["qt"].qtimageformats = True


        #This prevents protoc from needing zlib which adds a failing rpath protoc
        # self.options["protobuf"].with_zlib=False

        #This is survex dependancy
        #self.options["wxwidgets"].webview=False
        # self.options["wxwidgets"].shared=True
        # self.options["proj"].shared=True
#        self.options["tiff"].shared=True
#        self.options["proj"].with_tiff=False
        # self.options["zlib"].shared=True
