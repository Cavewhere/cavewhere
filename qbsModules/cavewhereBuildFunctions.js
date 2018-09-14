
var File = require("qbs.File");

function findIfExisting(paths, programName) {
    for(i in paths) {
        if(File.exists(paths[i])) {
            return paths[i]
        }
    }

    return "cant-find-" + programName
}
