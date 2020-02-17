import qbs 1.0
import qbs.File
import qbs.Process
import "cavewhereBuildFunctions.js" as utils

Probe {
    id: gitProb

    property path sourceDirectory
    readonly property string productVersion: ""
    readonly property string execPath: utils.findIfExisting(["C:/Program Files/Git/cmd/git.exe",
                                                             "/usr/local/bin/git",
                                                             "/usr/bin/git"
                                                            ],
                                                            "git")
    configure: {
        //Use git to query the version
        var git = execPath
        productVersion = "Unknown Version"
        found = false;

        if(File.exists(git)) {
            var gitProcess = new Process();
            gitProcess.setWorkingDirectory(sourceDirectory)
            gitProcess.exec(git, ["describe"] ,true);
            productVersion = gitProcess.readStdOut();
            productVersion = productVersion.replace(/(\r\n|\n|\r)/gm,""); //Remove newlines
            found = true
        }
    }
}
