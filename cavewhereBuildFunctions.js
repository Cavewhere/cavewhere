function findIfExisting(paths) {
    for(i in paths) {
        if(File.exists(paths[i])) {
            return paths[i]
        }
    }

    return "cant-find-git"
}
