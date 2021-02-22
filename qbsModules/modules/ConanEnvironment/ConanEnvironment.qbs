import qbs
import qbs.Environment
import qbs.TextFile

Module {
    id: rootId

    property string conanJsonFile

    property var _conan: conanJsonProbe.conanJson

    Probe {
        id: conanJsonProbe
        property var conanJson: {}
        property string conanfile: conanJsonFile
        configure: {
            console.info("Conan JSON for ConanEnvironment:" + conanfile)
            var fileHandle = TextFile(conanfile, TextFile.ReadOnly)
            var jsonText = fileHandle.readAll();
            conanJson = JSON.parse(jsonText);
            found = true
        }
    }

    setupRunEnvironment: {
        for(var envVar in product.ConanEnvironment._conan.deps_env_info) {
            var value = product.ConanEnvironment._conan.deps_env_info[envVar]
            if(Array.isArray(value)) {
                value = value.join(product.qbs.pathListSeparator)
            }
            console.info("env " + envVar + "=" + value);
            Environment.putEnv(envVar, value)
        }
    }
}
