from conans import ConanFile, CMake, tools
import os, sys

class CaveWhereConan(ConanFile):
    name = "CaveWhere"
    license = "MIT"
    author = "Philip Schuchardt vpicaver@gmail.com"
    url = "https://github.com/Cavewhere/Cavewhere"
    description = "Undergound Cave Survey and Mapping Software"
    topics = ("gis")
    version = "1.0"
    settings = "os", "compiler", "build_type", "arch"
    requires = [
    ("catch2/2.13.4"),
    ("protobuf/3.12.4"),
    ("survex/1.2.44@cave-software/dev"),
    ("dewalls/7e97092a144f153cb9ed7d318808208e9b35c74f@cave-software/dev"),
    ("sqlite3/3.34.1"),
    ("protobuf/3.12.4"),
    ("QbsToolchainProfile/1.0@cave-software/dev")
    ]

    options = {"system_qt": [True, False],
               "system_qbs": [True, False]}
    default_options = {"system_qt": True,
                      "system_qbs": True }
    generators = "qbs", "json"

    def requirements(self):
        # Or add a new requirement!
        if not self.options.system_qt:
           self.requires("qt/5.15.2")

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

    def configure(self):
        if not self.options.system_qt:
            self.options["qt"].shared = True

        self.options["dewalls"].system_qt = self.options.system_qt
        self.options["dewalls"].system_qbs = self.options.system_qbs

    def source(self):
        self.run("git clone  https://github.com/Cavewhere/dewalls.git --branch master .")
        self.run("git pull --tags")

    def build_requirements(self):
        if not self.options.system_qbs:
            self.build_requires("Qbs/1.17.0@cave-software/dev")
