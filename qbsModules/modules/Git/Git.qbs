import qbs 1.0
import qbs.File
import qbs.Process
import "cavewhereBuildFunctions.js" as utils

Module {
    name: "Git"

    readonly property string execPath: utils.findIfExisting(["C:/Program Files/Git/cmd/git.exe",
                                                             "C:/Program Files (x86)/Git/cmd/git.exe",
                                                             "/usr/bin/git",
                                                             "/usr/local/bin/git"],
                                                            "git")

    readonly property string productVersion: {
        //Use git to query the version
        var git = execPath
        var gitDescribe = "Unknown Version"

        if(File.exists(git)) {
            var gitProcess = new Process();
            gitProcess.setWorkingDirectory(product.sourceDirectory)
            gitProcess.exec(git, ["describe"] ,true);
            gitDescribe = gitProcess.readStdOut();
            gitDescribe = gitDescribe.replace(/(\r\n|\n|\r)/gm,""); //Remove newlines
        }

        return gitDescribe;
    }
}
