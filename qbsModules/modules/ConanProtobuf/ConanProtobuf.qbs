import qbs.FileInfo
import qbs.TextFile
import qbs.Probes
import qbs.ModUtils
import "conanProtobufUtils.js" as Utils

Module {
    id: rootId
    readonly property var protobufDepend: {
        var isProtoBuf = function(obj) {
            return obj.name === "protobuf";
        }

        function find(array, findFunction) {
            for(var i in array) {
                if(findFunction(array[i])) {
                    return array[i]
                }
            }
            return undefined
        }
        //console.error("Probe conan:" + conanJsonProbe + JSON.stringify(conanJsonProbe) + conanJsonProbe.found)
        if(conanJsonProbe.conanJson && conanJsonProbe.conanJson.dependencies) {
            //console.error("Depends:" + JSON.stringify(find(conanJsonProbe.conanJson.dependencies, isProtoBuf)))
            return find(conanJsonProbe.conanJson.dependencies, isProtoBuf)
        }
        return {}
    }

    readonly property string compilerPath: {
        //console.error("Probe:" + protocProbe.filePath)
        return protocProbe.filePath
    }

    property string conanJsonFile

    Probe {
        id: conanJsonProbe
        property var conanJson: {}
        property string conanfile: conanJsonFile
        configure: {
            console.info("Conan JSON:" + conanfile)
            var fileHandle = TextFile(conanfile, TextFile.ReadOnly)
            var jsonText = fileHandle.readAll();
            conanJson = JSON.parse(jsonText);
            found = true
        }
    }

    Probes.BinaryProbe {
        id: protocProbe
        searchPaths: Utils.fixWindowsPaths(protobufDepend.bin_paths)
        names: "protoc"
    }

    Depends { name: "cpp" }

    cpp.includePaths: Utils.fixWindowsPaths(protobufDepend.include_paths)
    cpp.libraryPaths: Utils.fixWindowsPaths(protobufDepend.lib_paths)
    cpp.dynamicLibraries: protobufDepend.libs


    FileTagger {
        patterns: ["*.proto"]
        fileTags: ["proto"]
    }

    Rule {
        id: protoCompiler
        inputs: ["proto"]
        inputsFromDependencies: ["application"]

        outputArtifacts: {
            var artifacts = [];

            for(var i in inputs.proto) {
                var baseName = FileInfo.baseName(inputs.proto[i].filePath)
                var fullPath = "serialization/" + baseName
                var headerPath = fullPath + ".pb.h"
                var srcPath = fullPath + ".pb.cc"

                var headerArtifact = { filePath: headerPath, fileTags: ["hpp"] }
                var srcArtifact = { filePath: srcPath, fileTags: ["cpp"] }

                artifacts.push(headerArtifact)
                artifacts.push(srcArtifact)
            }

            return artifacts
        }

        outputFileTags: ["hpp", "cpp"]

        prepare: {

            var commands = [];
            var protoc = input.ConanProtobuf.compilerPath //ConanProtobuf.compilerPath //product.protobufDepend.bin_paths[0].replace(/\\/g, "/") + "/protoc.exe" //"protoc-not-found"

            var proto_path = FileInfo.path(input.filePath)
            var cpp_out = product.buildDirectory + "/serialization"

            var protoPathArg = "--proto_path=" + proto_path
            var cppOutArg = "--cpp_out=" + cpp_out
            var inputFile = input.filePath

            var cmd = new Command(protoc,
                                  [protoPathArg, cppOutArg, inputFile])
            cmd.description = "Running protoc on " + inputFile + "with args " + protoc + " " + protoPathArg + " " + cppOutArg
            cmd.highlight = 'codegen'

            commands.push(cmd)
            return commands;
        }
    }
}
