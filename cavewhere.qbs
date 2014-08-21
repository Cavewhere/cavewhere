import qbs 1.0
import qbs.TextFile
import qbs.Process
import qbs.FileInfo

Project {
    Application {
        id: applicationId
        name: "Cavewhere"

        Depends { name: "cpp" }
        Depends { name: "Qt";
            submodules: [ "core", "gui", "widgets", "script", "quick", "sql", "opengl", "xml", "concurrent" ]
        }
        Depends { name: "QMath3d" }
//        Depends { name: "icns-out" }

        Qt.quick.qmlDebugging: true //qbs.buildVariant === "debug"

        cpp.includePaths: [
            "src",
            "QMath3d",
            "src/utils",
            product.buildDirectory + "/serialization",
            product.buildDirectory + "/versionInfo",
            "/usr/local/include"
        ]

        cpp.libraryPaths: [
            "/usr/local/lib"
        ]

        cpp.dynamicLibraries: [
            "squish",
            "z",
            "protobuf",
            "c++"
        ]

        cpp.frameworks: [
            "OpenGL"
        ]

        cpp.cxxFlags: [
            "-stdlib=libc++", //Needed for protoc
            "-Werror",
            "-std=c++11" //For c++11 support
        ]

        cpp.defines:[
            "TRILIBRARY",
            "ANSI_DECLARATORS"
        ]

        Properties {
            //This property is set so we can debug QML will in the application in
            //debug mode.
            condition: qbs.buildVariant == "debug"
            cpp.defines: outer.concat("CAVEWHERE_SOURCE_DIR=\"" + sourceDirectory + "\"")
        }

        cpp.infoPlistFile: "Info.plist"
        cpp.minimumOsxVersion: "10.7"

        Group {
            name: "ProtoFiles"

            files: [
                "src/cavewhere.proto",
                "src/qt.proto"
            ]

            fileTags: ["proto"]
        }

        Group {
            name: "cppFiles"
            files: [
                "src/main.cpp",
                "src/cwSurveyChunk.cpp",
                "src/cwStation.cpp",
                "src/cwShot.cpp",
                "src/cwSurvexImporter.cpp",
                "src/cwSurveyChunkGroupView.cpp",
                "src/cwClinoValidator.cpp",
                "src/cwStationValidator.cpp",
                "src/cwValidator.cpp",
                "src/cwCompassValidator.cpp",
                "src/cwDistanceValidator.cpp",
                "src/cwImageModel.cpp",
                "src/cwSurveyNoteModel.cpp",
                "src/cwNote.cpp",
                "src/cwTrip.cpp",
                "src/cwCave.cpp",
                "src/cwCavingRegion.cpp",
                "src/cwRegionTreeModel.cpp",
                "src/cwSurvexGlobalData.cpp",
                "src/cwSurvexBlockData.cpp",
                "src/cwSurvexImporterModel.cpp",
                "src/cwImportSurvexDialog.cpp",
                "src/cwGlobalIcons.cpp",
                "src/cwSurveyChunkViewComponents.cpp",
                "src/cwTask.cpp",
                "src/cwSurvexExporterTripTask.cpp",
                "src/cwTripStatistics.cpp",
                "src/cwSurvexExporterCaveTask.cpp",
                "src/cwSurvexExporterRegionTask.cpp",
                "src/cwLinePlotTask.cpp",
                "src/cwLinePlotManager.cpp",
                "src/cwCavernTask.cpp",
                "src/cwPlotSauceTask.cpp",
                "src/cwPlotSauceXMLTask.cpp",
                "src/cwUsedStationsTask.cpp",
                "src/cwUsedStationTaskManager.cpp",
                "src/cwCompassExporterRegionTask.cpp",
                "src/cwExporterTask.cpp",
                "src/cwCompassExporterCaveTask.cpp",
                "src/cwCaveExporterTask.cpp",
                "src/cwUnits.cpp",
                "src/cwPerson.cpp",
                "src/cwTeamMember.cpp",
                "src/cwTeam.cpp",
                "src/cwTripCalibration.cpp",
                "src/cwTaskProgressDialog.cpp",
                "src/cwStringListErrorModel.cpp",
                "src/cwGlobalUndoStack.cpp",
                "src/cwUndoer.cpp",
                "src/cwSurveyChunkView.cpp",
                "src/cwGLViewer.cpp",
                "src/cwCamera.cpp",
                "src/cwPlane.cpp",
                "src/cwLine3D.cpp",
                "src/cwMouseEventTransition.cpp",
                "src/cwGLShader.cpp",
                "src/cwShaderDebugger.cpp",
                "src/cwWheelEventTransition.cpp",
                "src/cwEdgeTile.cpp",
                "src/cwTile.cpp",
                "src/utils/Forsyth.cpp",
                "src/cwGLTerrain.cpp",
                "src/cwGLObject.cpp",
                "src/cwRegularTile.cpp",
                "src/cwLinePlotGeometryTask.cpp",
                "src/cwGLLinePlot.cpp",
                "src/cwCollisionRectKdTree.cpp",
                "src/cw3dRegionViewer.cpp",
                "src/cwImage.cpp",
                "src/cwImageData.cpp",
                "src/cwProject.cpp",
                "src/cwAddImageTask.cpp",
                "src/cwFileDialogHelper.cpp",
                "src/cwGunZipReader.cpp",
                "src/cwProjectIOTask.cpp",
                "src/cwRegionIOTask.cpp",
                "src/cwRegionSaveTask.cpp",
                "src/cwRegionLoadTask.cpp",
                "src/cwNoteStation.cpp",
                "src/cwNoteTranformation.cpp",
                "src/cwScrap.cpp",
                "src/cwTransformUpdater.cpp",
                "src/cwBaseNoteStationInteraction.cpp",
                "src/cwBaseScrapInteraction.cpp",
                "src/cwImageItem.cpp",
                "src/cwScrapItem.cpp",
                "src/cwBasePanZoomInteraction.cpp",
                "src/cwNoteInteraction.cpp",
                "src/cwScrapView.cpp",
                "src/cwScrapStationView.cpp",
                "src/cwLength.cpp",
                "src/cwGlobals.cpp",
                "src/cwInteraction.cpp",
                "src/cwQMLRegister.cpp",
                "src/cwNorthArrowItem.cpp",
                "src/cwPositioner3D.cpp",
                "src/cwScaleLengthItem.cpp",
                "src/cwAbstract2PointItem.cpp",
                "src/cwImageProperties.cpp",
                "src/cwTriangulateTask.cpp",
                "src/cwScrapManager.cpp",
                "src/cwTriangulateStation.cpp",
                "src/cwTriangulateInData.cpp",
                "src/cwTriangulatedData.cpp",
                "src/cwCropImageTask.cpp",
                "src/utils/cwTriangulate.cpp",
                "src/cwGLScraps.cpp",
                "src/cwGlobalDirectory.cpp",
                "src/cwImageTexture.cpp",
                "src/cwSurveyChunkTrimmer.cpp",
                "src/cwSurveyExportManager.cpp",
                "src/cwRootData.cpp",
                "src/utils/sqlite3.c",
                "src/cwItemSelectionModel.cpp",
                "src/cwStationPositionLookup.cpp",
                "src/cwSurveyImportManager.cpp",
                "src/cwTripLengthTask.cpp",
                "src/cwSurvexLRUDChunk.cpp",
                "src/cwGLGridPlane.cpp",
                "src/cwLabel3dView.cpp",
                "src/cwLabel3dItem.cpp",
                "src/cwLinePlotLabelView.cpp",
                "src/cwLabel3dGroup.cpp",
                "src/cwSGPolygonNode.cpp",
                "src/cwSGLinesNode.cpp",
                "src/cwAbstractPointManager.cpp",
                "src/cwScrapPointView.cpp",
                "src/cwScrapOutlinePointView.cpp",
                "src/cwSelectionManager.cpp",
                "src/cwUnitValue.cpp",
                "src/cwImageResolution.cpp",
                "src/cwRemoveImageTask.cpp",
                "src/cwGeometryItersecter.cpp",
                "src/cwCompassImporter.cpp",
                "src/cwImageCleanupTask.cpp",
                "src/cwExportRegionViewerToImageTask.cpp",
                "src/cwProjection.cpp",
                "src/cwAbstractProjection.cpp",
                "src/cwOrthogonalProjection.cpp",
                "src/cwPerspectiveProjection.cpp",
                "src/cwMatrix4x4Animation.cpp",
                "src/cwTextureUploadTask.cpp",
                "src/cwImageProvider.cpp",
                "src/cwImageValidator.cpp",
                "src/cwGLResources.cpp",
                "src/cwGLImageItemResources.cpp",
                "src/cwQMLReload.cpp",
                "src/cwCompassItem.cpp",
                "src/cwLicenseAgreement.cpp",
                "src/cwOpenFileEventHandler.cpp",
                "src/cwSurveyChunk.h",
                "src/cwStation.h",
                "src/cwShot.h",
                "src/cwSurvexImporter.h",
                "src/cwSurveyChunkGroupView.h",
                "src/cwClinoValidator.h",
                "src/cwStationValidator.h",
                "src/cwValidator.h",
                "src/cwCompassValidator.h",
                "src/cwDistanceValidator.h",
                "src/cwImageModel.h",
                "src/cwSurveyNoteModel.h",
                "src/cwNote.h",
                "src/cwTrip.h",
                "src/cwCave.h",
                "src/cwCavingRegion.h",
                "src/cwRegionTreeModel.h",
                "src/cwSurvexGlobalData.h",
                "src/cwSurvexBlockData.h",
                "src/cwSurvexImporterModel.h",
                "src/cwImportSurvexDialog.h",
                "src/cwGlobalIcons.h",
                "src/cwSurveyChunkViewComponents.h",
                "src/cwTask.h",
                "src/cwSurvexExporterTripTask.h",
                "src/cwTripStatistics.h",
                "src/cwSurvexExporterCaveTask.h",
                "src/cwSurvexExporterRegionTask.h",
                "src/cwLinePlotTask.h",
                "src/cwLinePlotManager.h",
                "src/cwCavernTask.h",
                "src/cwPlotSauceTask.h",
                "src/cwPlotSauceXMLTask.h",
                "src/cwUsedStationsTask.h",
                "src/cwUsedStationTaskManager.h",
                "src/cwCompassExporterRegionTask.h",
                "src/cwExporterTask.h",
                "src/cwCompassExporterCaveTask.h",
                "src/cwCaveExporterTask.h",
                "src/cwUnits.h",
                "src/cwPerson.h",
                "src/cwTeamMember.h",
                "src/cwTeam.h",
                "src/cwTripCalibration.h",
                "src/cwTaskProgressDialog.h",
                "src/cwStringListErrorModel.h",
                "src/cwGlobalUndoStack.h",
                "src/cwUndoer.h",
                "src/cwSurveyChunkView.h",
                "src/cwGLViewer.h",
                "src/cwCamera.h",
                "src/cwPlane.h",
                "src/cwLine3D.h",
                "src/cwMouseEventTransition.h",
                "src/cwGLShader.h",
                "src/cwDebug.h",
                "src/cwShaderDebugger.h",
                "src/cwWheelEventTransition.h",
                "src/cwEdgeTile.h",
                "src/cwTile.h",
                "src/utils/vcacheopt.h",
                "src/utils/Forsyth.h",
                "src/cwGLTerrain.h",
                "src/cwGLObject.h",
                "src/cwRegularTile.h",
                "src/cwLinePlotGeometryTask.h",
                "src/cwGLLinePlot.h",
                "src/cwCollisionRectKdTree.h",
                "src/cw3dRegionViewer.h",
                "src/cwImage.h",
                "src/cwImageData.h",
                "src/cwProject.h",
                "src/cwAddImageTask.h",
                "src/cwFileDialogHelper.h",
                "src/cwGunZipReader.h",
                "src/cwSerialization.h",
                "src/cwQtSerialization.h",
                "src/cwProjectIOTask.h",
                "src/cwRegionIOTask.h",
                "src/cwRegionSaveTask.h",
                "src/cwRegionLoadTask.h",
                "src/cwNoteStation.h",
                "src/cwNoteTranformation.h",
                "src/cwScrap.h",
                "src/cwTransformUpdater.h",
                "src/cwBaseNoteStationInteraction.h",
                "src/cwBaseScrapInteraction.h",
                "src/cwImageItem.h",
                "src/cwScrapItem.h",
                "src/cwBasePanZoomInteraction.h",
                "src/cwNoteInteraction.h",
                "src/cwScrapView.h",
                "src/cwScrapStationView.h",
                "src/cwLength.h",
                "src/cwGlobals.h",
                "src/cwInteraction.h",
                "src/cwQMLRegister.h",
                "src/cwNorthArrowItem.h",
                "src/cwPositioner3D.h",
                "src/cwScaleLengthItem.h",
                "src/cwAbstract2PointItem.h",
                "src/cwImageProperties.h",
                "src/cwTriangulateTask.h",
                "src/cwScrapManager.h",
                "src/cwTriangulateStation.h",
                "src/cwTriangulateInData.h",
                "src/cwTriangulatedData.h",
                "src/cwCropImageTask.h",
                "src/utils/cwTriangulate.h",
                "src/cwGLScraps.h",
                "src/cwGlobalDirectory.h",
                "src/cwImageTexture.h",
                "src/cwReadingStates.h",
                "src/cwSurveyChunkTrimmer.h",
                "src/cwSurveyExportManager.h",
                "src/cwRootData.h",
                "src/cwMath.h",
                "src/cwItemSelectionModel.h",
                "src/cwStationPositionLookup.h",
                "src/cwSurveyImportManager.h",
                "src/cwTripLengthTask.h",
                "src/cwSurvexLRUDChunk.h",
                "src/cwGLGridPlane.h",
                "src/cwLabel3dView.h",
                "src/cwLabel3dItem.h",
                "src/cwLinePlotLabelView.h",
                "src/cwLabel3dGroup.h",
                "src/cwSGPolygonNode.h",
                "src/cwSGLinesNode.h",
                "src/cwAbstractPointManager.h",
                "src/cwScrapPointView.h",
                "src/cwScrapOutlinePointView.h",
                "src/cwSelectionManager.h",
                "src/cwUnitValue.h",
                "src/cwImageResolution.h",
                "src/cwRemoveImageTask.h",
                "src/cwGeometryItersecter.h",
                "src/cwCompassImporter.h",
                "src/cwImageCleanupTask.h",
                "src/cwExportRegionViewerToImageTask.h",
                "src/cwProjection.h",
                "src/cwAbstractProjection.h",
                "src/cwOrthogonalProjection.h",
                "src/cwPerspectiveProjection.h",
                "src/cwMatrix4x4Animation.h",
                //            "src/serialization/cavewhere.pb.cc",
                //            "src/serialization/qt.pb.cc",
                "src/cwTextureUploadTask.h",
                "src/cwImageProvider.h",
                "src/cwImageValidator.h",
                "src/cwGLResources.h",
                "src/cwGLImageItemResources.h",
                "src/cwQMLReload.h",
                "src/cwCompassItem.h",
                "src/cwLicenseAgreement.h",
                "src/cwOpenFileEventHandler.h",
                "src/cwInitCommand.h",
                "src/cwInitCommand.cpp",
                "src/cwUpdateDataCommand.h",
                "src/cwUpdateDataCommand.cpp",
                "src/cwScene.h",
                "src/cwScene.cpp",
                "src/cwInitializeOpenGLFunctionsCommand.h",
                "src/cwInitializeOpenGLFunctionsCommand.cpp",
                "src/cwSceneCommand.h",
                "src/cwSceneCommand.cpp",
                "src/cwRegionSceneManager.h",
                "src/cwRegionSceneManager.cpp",
                "src/cwScale.h",
                "src/cwScale.cpp",
                "src/cwCaptureManager.cpp",
                "src/cwCaptureManager.h",
                "src/cwScreenCaptureCommand.cpp",
                "src/cwScreenCaptureCommand.h",
                "src/cwGraphicsImageItem.cpp",
                "src/cwGraphicsImageItem.h",
                "src/cwBaseTurnTableInteraction.h",
                "src/cwBaseTurnTableInteraction.cpp",
                "src/cwQuickSceneView.h",
                "src/cwQuickSceneView.cpp",
                "src/cwViewportCapture.h",
                "src/cwViewportCapture.cpp"
            ]
        }

        Group {
            name: "qmlFiles"
            files: [
                "qml/DataSideBar.qml",
                "qml/CompactTabWidget.qml",
                "qml/Button.qml",
                "qml/CaveDataSidebarPage.qml",
                "qml/StandardBorder.qml",
                "qml/DataSidebarItemTab.qml",
                "qml/TreeRootElement.qml",
                "qml/UsedStationsWidget.qml",
                "qml/AllCavesTabWidget.qml",
                "qml/CaveTabWidget.qml",
                "qml/TripTabWidget.qml",
                "qml/CaveTreeDelegate.qml",
                "qml/IconButton.qml",
                "qml/GLTerrainRenderer.qml",
                "qml/FileDialog.qml",
                "qml/ButtonGroup.qml",
                "qml/Splitter.qml",
                "qml/NoteStation.qml",
                "qml/PanZoomInteraction.qml",
                "qml/NoteItem.qml",
                "qml/HelpBox.qml",
                "qml/NoteTransformEditor.qml",
                "qml/ScrapInteraction.qml",
                "qml/NoteStationInteraction.qml",
                "qml/NoteItemSelectionInteraction.qml",
                "qml/NoteNorthInteraction.qml",
                "qml/InteractionManager.qml",
                "qml/NoteScaleInteraction.qml",
                "qml/HelpArea.qml",
                "qml/LabelWithHelp.qml",
                "qml/LabelValueUnit.qml",
                "qml/ClickTextInput.qml",
                "qml/DoubleClickTextInput.qml",
                "qml/Style.qml",
                "qml/CoreClickTextInput.qml",
                "qml/PaperScaleInput.qml",
                "qml/Pallete.qml",
                "qml/UnitInput.qml",
                "qml/NoteNorthUpInput.qml",
                "qml/GlobalShadowTextInput.qml",
                "qml/DataBox.qml",
                "qml/Utils.js",
                "qml/CavewhereMainWindow.qml",
                "qml/SurveyEditor.qml",
                "qml/StationBox.qml",
                "qml/ShotDistanceDataBox.qml",
                "qml/ClinoReadBox.qml",
                "qml/CompassReadBox.qml",
                "qml/ReadingBox.qml",
                "qml/Navigation.js",
                "qml/ImageExplorer.qml",
                "qml/NoteExplorer.qml",
                "qml/NotesGallery.qml",
                "qml/ImageArrowNavigation.qml",
                "qml/FrontSightReadingBox.qml",
                "qml/BackSightReadingBox.qml",
                "qml/TeamTable.qml",
                "qml/SectionLabel.qml",
                "qml/BreakLine.qml",
                "qml/RemoveButton.qml",
                "qml/AddButton.qml",
                "qml/CalibrationEditor.qml",
                "qml/CheckableGroupBox.qml",
                "qml/GroupBox.qml",
                "qml/TapeCalibrationEditor.qml",
                "qml/FrontSightCalibrationEditor.qml",
                "qml/BackSightCalibrationEditor.qml",
                "qml/DeclainationEditor.qml",
                "qml/InformationButton.qml",
                "qml/ErrorHelpArea.qml",
                "qml/ErrorHelpBox.qml",
                "qml/DataMainPage.qml",
                "qml/ExportSurveyMenuItem.qml",
                "qml/ShadowRectangle.qml",
                "qml/TitleLabel.qml",
                "qml/CaveDataToolbar.qml",
                "qml/Menu.qml",
                "qml/ContextMenu.qml",
                "qml/MenuItem.qml",
                "qml/GlobalMenuMouseHandler.qml",
                "qml/Label3d.qml",
                "qml/FileButtonAndMenu.qml",
                "qml/ScrapOutlinePoint.qml",
                "qml/PointItem.qml",
                "qml/ScrapPointItem.qml",
                "qml/ScrapPointMouseArea.qml",
                "qml/FloatingGroupBox.qml",
                "qml/NoteResolution.qml",
                "qml/UnitValueInput.qml",
                "qml/CaveOverviewPage.qml",
                "qml/CaveLengthComponent.qml",
                "qml/CaveLengthAndDepth.qml",
                "qml/ContextMenuButton.qml",
                "qml/RemoveDataRectangle.qml",
                "qml/ProjectionSlider.qml",
                "qml/ToggleSlider.qml",
                "qml/RemoveAskBox.qml",
                "qml/ScaleBar.qml",
                "qml/AboutWindow.qml",
                "qml/LicenseWindow.qml",
                "qml/CavewhereLogo.qml",
                "qml/LoadNotesWidget.qml",
                "qml/LoadNotesIconButton.qml",
                "qml/CameraOptionsTab.qml",
                "qml/RenderingSideBar.qml",
                "qml/RenderingView.qml",
                "qml/CameraAzimuthSettings.qml",
                "qml/CameraVerticalAngleSettings.qml",
                "qml/CameraProjectionSettings.qml",
                "qml/ChoosePaperSizeInteraction.qml",
                "qml/ExportViewTab.qml",
                "qml/TurnTableInteraction.qml",
                "qml/SelectExportAreaInteraction.qml",
                "qml/SelectExportAreaTool.qml",
                "qml/SelectionRectangle.qml"

            ]

        }

        Group {
            name: "shaderFiles"

            files: [
                "shaders/simple.vert",
                "shaders/simple.frag",
                "shaders/LinePlot.vert",
                "shaders/LinePlot.frag",
                "shaders/NoteItem.frag",
                "shaders/NoteItem.vert",
                "shaders/tileVertex.vert",
                "shaders/tileVertex.frag",
                "shaders/tileVertex.geom",
                "shaders/grid.vert",
                "shaders/grid.frag",
                "shaders/scrap.vert",
                "shaders/scrap.frag",
                "shaders/scrap.geom",
                "shaders/compass/compass.vsh",
                "shaders/compass/compass.fsh",
                "shaders/compass/compassShadowX.vsh",
                "shaders/compass/compassShadow.fsh",
                "shaders/compass/compassShadowY.vsh",
                "shaders/compass/compassShadowOutput.vsh",
                "shaders/compass/compassShadowOutput.fsh"
            ]
        }

        Group {
            name: "uiForms"
            files: [
                "src/cwImportSurvexDialog.ui",
                "src/cwTaskProgressDialog.ui"
            ]
        }

        Group {
            name: "packageCreatorScripts"

            files: [
                "installer/mac/installMac.sh"
            ]
        }



        Group {
            name: "DocumentationFiles"
            files: [
                "docs/FileFormatDocumentation.txt",
                "LICENSE.txt"
            ]
        }

        Group {
            name: "rcFiles"
            files: [
                "Cavewhere.rc"
            ]
        }

        Group {
            name: "qrcFiles"
            files: [
                "resources.qrc"
            ]
        }

        Group {
            name: "macIcons"
            files: [
                "cavewhereIcon.icns",
            ]
            fileTags: ["icns-in"]
        }

        Group {
            name: "survexDepends"
            files: [
                "plotsauce",
                "cavern",
                "share"
            ]
            fileTags: ["survex"]
        }

//        Group {
//            name: "macInfo"
//            files: [
//                "Info.plist"
//            ]
//        }

        Rule {
            id: macIconCopier
            inputs: ["icns-in"]
            auxiliaryInputs: ["application"]

            Artifact {
                fileTags: ["resourcerules"]
                filePath: product.buildDirectory + "/Cavewhere.app/Contents/Resources/" + FileInfo.baseName(input.filePath) + ".icns"
//                fileName: applicationId.name + ".app/Contents/Resources/" + FileInfo.baseName(input.filePath) + ".icns"
            }

            prepare: {
                print("Preparing" + input.filePath + " to " + output.filePath)
                var cp = "/bin/cp"
                var realOutputFile = product.buildDirectory + "/Cavewhere.app/Contents/Resources/" + FileInfo.baseName(input.filePath) + ".icns"
                var cmd = new Command(cp,
                                      [input.filePath, realOutputFile])
                cmd.description = "Copying icons to resources " + input.filePath + "to" + output.filePath
                cmd.highlight = 'codegen'
                return cmd
            }
        }

        Rule {
            id: survexCopier
            inputs: ["survex"]
            auxiliaryInputs: ["application"]

            Artifact {
                fileTags: ["resourcerules"]
                filePath: product.buildDirectory + "/Cavewhere.app/Contents/MacOS/" + FileInfo.baseName(input.filePath)
//                fileName: applicationId.name + ".app/Contents/Resources/" + FileInfo.baseName(input.filePath) + ".icns"
            }

            prepare: {
                print("Preparing" + input.filePath + " to " + output.filePath)
                var cp = "/bin/cp"
                var realOutputFile = product.buildDirectory + "/Cavewhere.app/Contents/MacOS/" + FileInfo.baseName(input.filePath)
                var cmd = new Command(cp,
                                      ["-r", input.filePath, realOutputFile])
                cmd.description = "Copying survex " + input.filePath + "to" + output.filePath
                cmd.highlight = 'codegen'
                return cmd
            }
        }

        Rule {
            id: protoCompiler
            inputs: ["proto"]

            Artifact {
                fileTags: ["hpp"]
                filePath: "serialization/" + FileInfo.baseName(input.filePath) + ".pb.h"
            }

            Artifact {
                fileTags: ["cpp"]
                filePath: "serialization/" + FileInfo.baseName(input.filePath) + ".pb.cc"
            }

            prepare: {
                var protoc = "/usr/local/bin/protoc"
                var proto_path = FileInfo.path(input.filePath)
                var cpp_out = product.buildDirectory + "/serialization"

                var protoPathArg = "--proto_path=" + proto_path
                var cppOutArg = "--cpp_out=" + cpp_out

                var cmd = new Command(protoc,
                                      [protoPathArg, cppOutArg, input.filePath])
                cmd.description = "Running protoc on " + input.filePath + "with args " + protoPathArg + " " + cppOutArg
                cmd.highlight = 'codegen'
                return cmd;
            }
        }

        Transformer {
            id: cavewhereVersionGenerator

            Artifact {
                fileTags: ["hpp"]
                filePath: "versionInfo/cavewhereVersion.h"
            }

            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "generating version info in" + output.filePath;

                //Broken for now, but once next version of qbs comes out, this should work.
                //See https://bugreports.qt-project.org/browse/QBS-385
                //Use git to query the version
                var git = "/usr/bin/git"
                var gitProcess = new Process();
                gitProcess.setWorkingDirectory(product.sourceDirectory)
                gitProcess.exec(git, ["describe"] ,true);
                var gitDescribe = gitProcess.readStdOut();
                gitDescribe = gitDescribe.replace(/(\r\n|\n|\r)/gm,""); //Remove newlines
                cmd.cavewhereVersion = gitDescribe

                cmd.sourceCode = function() {
                    var all = "#ifndef cavewhereVersion_H\n #define cavewhereVersion_H\n static const QString CavewhereVersion = \"" + cavewhereVersion + "\";\n #endif\n\n";
                    var file = new TextFile(output.filePath, TextFile.WriteOnly);
                    file.write(all);
                    file.close();
                }
                return cmd;
            }
        }


    }


    DynamicLibrary {
        name: "QMath3d"
        Depends { name: "cpp" }
        Depends { name: "Qt";
            submodules: [ "core", "gui" ]
        }
        files: [
            "QMath3d/qbox3d.h",
            "QMath3d/qplane3d.h",
            "QMath3d/qray3d.h",
            "QMath3d/qsphere3d.h",
            "QMath3d/qtriangle3d.h",
            "QMath3d/qbox3d.cpp",
            "QMath3d/qplane3d.cpp",
            "QMath3d/qray3d.cpp",
            "QMath3d/qsphere3d.cpp",
            "QMath3d/qtriangle3d.cpp",
        ]
    }
}
