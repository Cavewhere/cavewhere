//import qbs 1.0
//import qbs.Process
//import qbs.Fileinfo

Module {
//    name: "protoBuffer"

    Depends { name: "cpp" }

    property path protoc
    property path cpp_out
    property path proto_path

    FileTagger {
        pattern: "*.proto"
        fileTags: ["proto"]
    }

    Rule {
        id: protoCompiler
        inputs: ["proto"]

        Artifact {
            fileTags: ["hpp"]
            fileName: FileInfo.baseName (input. fileName) + ".pb.h"
        }

        Artifact {
            fileTags: ["cpp"]
            fileName: FileInfo.baseName (input. fileName) + ".pb.cc"
        }

        prepare: {
            print("Applying protoc rules")
            var protoPathArg = "--proto_path=" + proto_path
            var cppOutArg = "--cpp_out=" + ccp_out

            var cmd = new Command(protoc,
                                  [protoPathArg, cppOutArg])
            cmd.description = "protoc " + FileInfo.filename(input.filename)
            cmd.highlight = 'codegen'
            return cmd;
        }
    }
}
