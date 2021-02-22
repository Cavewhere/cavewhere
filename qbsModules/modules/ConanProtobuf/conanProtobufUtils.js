function fixWindowsPath(path) {
    return path.replace(/\\/g, "/");
}

function fixWindowsPaths(pathArray) {
    for(var i in pathArray) {
        pathArray[i] = fixWindowsPath(pathArray[i])
    }
    return pathArray;
}
