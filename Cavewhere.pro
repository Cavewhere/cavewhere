#-------------------------------------------------
#
# Project created by QtCreator 2010-08-19T19:44:25
#
#-------------------------------------------------

TARGET = Cavewhere
TEMPLATE = app

   #Extra modules
QT += core sql concurrent xml qml quick opengl svg

OBJECTS_DIR = .obj
UI_DIR = .ui
MOC_DIR = .moc

#Defines for triangle
DEFINES += TRILIBRARY ANSI_DECLARATORS

SOURCES += src/main.cpp \
    src/cwSurveyChunk.cpp \
    src/cwStation.cpp \
    src/cwShot.cpp \
    src/cwSurvexImporter.cpp \
    src/cwSurveyChunkGroupView.cpp \
    src/cwClinoValidator.cpp \
    src/cwStationValidator.cpp \
    src/cwValidator.cpp \
    src/cwCompassValidator.cpp \
    src/cwDistanceValidator.cpp \
    src/cwImageModel.cpp \
    src/cwSurveyNoteModel.cpp \
    src/cwNote.cpp \
    src/cwTrip.cpp \
    src/cwCave.cpp \
    src/cwCavingRegion.cpp \
    src/cwRegionTreeModel.cpp \
    src/cwSurvexGlobalData.cpp \
    src/cwSurvexBlockData.cpp \
    src/cwSurvexImporterModel.cpp \
    src/cwImportSurvexDialog.cpp \
    src/cwGlobalIcons.cpp \
    src/cwSurveyChunkViewComponents.cpp \
    src/cwTask.cpp \
    src/cwSurvexExporterTripTask.cpp \
    src/cwTripStatistics.cpp \
    src/cwSurvexExporterCaveTask.cpp \
    src/cwSurvexExporterRegionTask.cpp \
    src/cwLinePlotTask.cpp \
    src/cwLinePlotManager.cpp \
    src/cwCavernTask.cpp \
    src/cwPlotSauceTask.cpp \
    src/cwPlotSauceXMLTask.cpp \
    src/cwUsedStationsTask.cpp \
    src/cwUsedStationTaskManager.cpp \
    src/cwCompassExporterRegionTask.cpp \
    src/cwExporterTask.cpp \
    src/cwCompassExporterCaveTask.cpp \
    src/cwCaveExporterTask.cpp \
    src/cwUnits.cpp \
    src/cwPerson.cpp \
    src/cwTeamMember.cpp \
    src/cwTeam.cpp \
    src/cwTripCalibration.cpp \
    src/cwTaskProgressDialog.cpp \
    src/cwStringListErrorModel.cpp \
    src/cwGlobalUndoStack.cpp \
    src/cwUndoer.cpp \
    src/cwSurveyChunkView.cpp \
    src/cwGLRenderer.cpp \
    src/cwCamera.cpp \
    src/cwPlane.cpp \
    src/cwLine3D.cpp \
    src/cwMouseEventTransition.cpp \
    src/cwGLShader.cpp \
    src/cwShaderDebugger.cpp \
    src/cwWheelEventTransition.cpp \
    src/cwEdgeTile.cpp \
    src/cwTile.cpp \
    src/utils/Forsyth.cpp \
    src/cwGLTerrain.cpp \
    src/cwGLObject.cpp \
    src/cwRegularTile.cpp \
    src/cwLinePlotGeometryTask.cpp \
    src/cwGLLinePlot.cpp \
    src/cwCollisionRectKdTree.cpp \
    src/cw3dRegionViewer.cpp \
    src/cwImage.cpp \
    src/cwImageData.cpp \
    src/cwProject.cpp \
    src/cwAddImageTask.cpp \
    src/cwFileDialogHelper.cpp \
    src/cwGunZipReader.cpp \
    src/cwProjectIOTask.cpp \
    src/cwRegionIOTask.cpp \
    src/cwRegionSaveTask.cpp \
    src/cwRegionLoadTask.cpp \
    src/cwNoteStation.cpp \
    src/cwNoteTranformation.cpp \
    src/cwScrap.cpp \
    src/cwTransformUpdater.cpp \
    src/cwBaseNoteStationInteraction.cpp \
    src/cwBaseScrapInteraction.cpp \
    src/cwImageItem.cpp \
    src/cwScrapItem.cpp \
    src/cwBasePanZoomInteraction.cpp \
#    src/cwWheelArea.cpp \
    src/cwNoteInteraction.cpp \
    src/cwScrapView.cpp \
    src/cwScrapStationView.cpp \
    src/cwLength.cpp \
    src/cwGlobals.cpp \
    src/cwInteraction.cpp \
    src/cwQMLRegister.cpp \
    src/cwNorthArrowItem.cpp \
    src/cwPositioner3D.cpp \
#    src/cwMainWindow.cpp \
    src/cwScaleLengthItem.cpp \
    src/cwAbstract2PointItem.cpp \
    src/cwImageProperties.cpp \
    src/cwTriangulateTask.cpp \
    src/cwScrapManager.cpp \
    src/cwTriangulateStation.cpp \
    src/cwTriangulateInData.cpp \
    src/cwTriangulatedData.cpp \
    src/cwCropImageTask.cpp \
    src/utils/cwTriangulate.cpp \
    src/cwGLScraps.cpp \
    src/cwGlobalDirectory.cpp \
    src/cwImageTexture.cpp \
    src/cwSurveyChunkTrimmer.cpp \
    src/cwSurveyExportManager.cpp \
    src/cwRootData.cpp \
    src/utils/sqlite3.c \
    src/cwItemSelectionModel.cpp \
    src/cwStationPositionLookup.cpp \
    src/cwSurveyImportManager.cpp \
    src/cwTripLengthTask.cpp \
    src/cwSurvexLRUDChunk.cpp \
    src/cwGLGridPlane.cpp \
    src/cwLabel3dView.cpp \
    src/cwLabel3dItem.cpp \
    src/cwLinePlotLabelView.cpp \
    src/cwLabel3dGroup.cpp \
    src/cwSGPolygonNode.cpp \
    src/cwSGLinesNode.cpp \
    src/cwAbstractPointManager.cpp \
    src/cwScrapPointView.cpp \
    src/cwScrapOutlinePointView.cpp \
    src/cwSelectionManager.cpp \
    src/cwUnitValue.cpp \
    src/cwImageResolution.cpp \
    src/cwRemoveImageTask.cpp \
    src/cwGeometryItersecter.cpp \
    src/cwCompassImporter.cpp \
    src/cwImageCleanupTask.cpp \
    src/cwExportRegionViewerToImageTask.cpp \
    src/cwProjection.cpp \
    src/cwAbstractProjection.cpp \
    src/cwOrthogonalProjection.cpp \
    src/cwPerspectiveProjection.cpp \
    src/cwMatrix4x4Animation.cpp \
    src/cwTextureUploadTask.cpp \
    src/cwImageProvider.cpp \
    src/cwImageValidator.cpp \
    src/cwGLResources.cpp \
    src/cwGLImageItemResources.cpp \
    src/cwQMLReload.cpp \
    src/cwCompassItem.cpp \
    src/cwLicenseAgreement.cpp \
    src/cwOpenFileEventHandler.cpp

HEADERS  += \
    src/cwSurveyChunk.h \
    src/cwStation.h \
    src/cwShot.h \
    src/cwSurvexImporter.h \
    src/cwSurveyChunkGroupView.h \
    src/cwClinoValidator.h \
    src/cwStationValidator.h \
    src/cwValidator.h \
    src/cwCompassValidator.h \
    src/cwDistanceValidator.h \
    src/cwImageModel.h \
    src/cwSurveyNoteModel.h \
    src/cwNote.h \
    src/cwTrip.h \
    src/cwCave.h \
    src/cwCavingRegion.h \
    src/cwRegionTreeModel.h \
    src/cwSurvexGlobalData.h \
    src/cwSurvexBlockData.h \
    src/cwSurvexImporterModel.h \
    src/cwImportSurvexDialog.h \
    src/cwGlobalIcons.h \
    src/cwSurveyChunkViewComponents.h \
    src/cwTask.h \
    src/cwSurvexExporterTripTask.h \
    src/cwTripStatistics.h \
    src/cwSurvexExporterCaveTask.h \
    src/cwSurvexExporterRegionTask.h \
    src/cwLinePlotTask.h \
    src/cwLinePlotManager.h \
    src/cwCavernTask.h \
    src/cwPlotSauceTask.h \
    src/cwPlotSauceXMLTask.h \
    src/cwUsedStationsTask.h \
    src/cwUsedStationTaskManager.h \
    src/cwCompassExporterRegionTask.h \
    src/cwExporterTask.h \
    src/cwCompassExporterCaveTask.h \
    src/cwCaveExporterTask.h \
    src/cwUnits.h \
    src/cwPerson.h \
    src/cwTeamMember.h \
    src/cwTeam.h \
    src/cwTripCalibration.h \
    src/cwTaskProgressDialog.h \
    src/cwStringListErrorModel.h \
    src/cwGlobalUndoStack.h \
    src/cwUndoer.h \
    src/cwSurveyChunkView.h \
    src/cwGLRenderer.h \
    src/cwCamera.h \
    src/cwPlane.h \
    src/cwLine3D.h \
    src/cwMouseEventTransition.h \
    src/cwGLShader.h \
    src/cwDebug.h \
    src/cwShaderDebugger.h \
    src/cwWheelEventTransition.h \
    src/cwEdgeTile.h \
    src/cwTile.h \
    src/utils/vcacheopt.h \
    src/utils/Forsyth.h \
    src/cwGLTerrain.h \
    src/cwGLObject.h \
    src/cwRegularTile.h \
    src/cwLinePlotGeometryTask.h \
    src/cwGLLinePlot.h \
    src/cwCollisionRectKdTree.h \
    src/cw3dRegionViewer.h \
    src/cwImage.h \
    src/cwImageData.h \
    src/cwProject.h \
    src/cwAddImageTask.h \
    src/cwFileDialogHelper.h \
    src/cwGunZipReader.h \
    src/cwSerialization.h \
    src/cwQtSerialization.h \
    src/cwProjectIOTask.h \
    src/cwRegionIOTask.h \
    src/cwRegionSaveTask.h \
    src/cwRegionLoadTask.h \
    src/cwNoteStation.h \
    src/cwNoteTranformation.h \
    src/cwScrap.h \
    src/cwTransformUpdater.h \
    src/cwBaseNoteStationInteraction.h \
    src/cwBaseScrapInteraction.h \
    src/cwImageItem.h \
    src/cwScrapItem.h \
    src/cwBasePanZoomInteraction.h \
#    src/cwWheelArea.h \
    src/cwNoteInteraction.h \
    src/cwScrapView.h \
    src/cwScrapStationView.h \
    src/cwLength.h \
    src/cwGlobals.h \
    src/cwInteraction.h \
    src/cwQMLRegister.h \
    src/cwQMLRegister.h \
    src/cwNorthArrowItem.h \
    src/cwPositioner3D.h \
#    src/cwMainWindow.h \
    src/cwScaleLengthItem.h \
    src/cwAbstract2PointItem.h \
    src/cwImageProperties.h \
    src/cwTriangulateTask.h \
    src/cwScrapManager.h \
    src/cwTriangulateStation.h \
    src/cwTriangulateInData.h \
    src/cwTriangulatedData.h \
    src/cwCropImageTask.h \
    src/utils/cwTriangulate.h \
    src/cwGLScraps.h \
    src/cwGlobalDirectory.h \
    src/cwImageTexture.h \
    src/cwReadingStates.h \
    src/cwSurveyChunkTrimmer.h \
    src/cwSurveyExportManager.h \
    src/cwRootData.h \
    src/cwMath.h \
    src/cwItemSelectionModel.h \
    src/cwStationPositionLookup.h \
    src/cwSurveyImportManager.h \
    src/cwTripLengthTask.h \
    src/cwSurvexLRUDChunk.h \
    src/cwGLGridPlane.h \
    src/cwLabel3dView.h \
    src/cwLabel3dItem.h \
    src/cwLinePlotLabelView.h \
    src/cwLabel3dGroup.h \
    src/cwSGPolygonNode.h \
    src/cwSGLinesNode.h \
    src/cwAbstractPointManager.h \
    src/cwScrapPointView.h \
    src/cwScrapOutlinePointView.h \
    src/cwSelectionManager.h \
    src/cwUnitValue.h \
    src/cwImageResolution.h \
    src/cwRemoveImageTask.h \
    src/cwGeometryItersecter.h \
    src/cwCompassImporter.h \
    src/cwImageCleanupTask.h \
    src/cwExportRegionViewerToImageTask.h \
    src/cwProjection.h \
    src/cwAbstractProjection.h \
    src/cwOrthogonalProjection.h \
    src/cwPerspectiveProjection.h \
    src/cwMatrix4x4Animation.h \
    src/serialization/cavewhere.pb.h \
    src/serialization/qt.pb.h \
    src/cwTextureUploadTask.h \
    src/cwImageProvider.h \
    src/cwImageValidator.h \
    src/cwGLResources.h \
    src/cwGLImageItemResources.h \
    src/cwQMLReload.h \
    src/cwCompassItem.h \
    src/cwLicenseAgreement.h \
    src/cwOpenFileEventHandler.h

FORMS    += \ #src/cwMainWindow.ui \
    src/cwImportSurvexDialog.ui \
    src/cwTaskProgressDialog.ui

OTHER_FILES += \
    qml/DataSideBar.qml \
    qml/CompactTabWidget.qml \
    qml/Button.qml \
    qml/CaveDataSidebarPage.qml \
    qml/StandardBorder.qml \
    qml/DataSidebarItemTab.qml \
    qml/TreeRootElement.qml \
    qml/UsedStationsWidget.qml \
    qml/AllCavesTabWidget.qml \
    qml/CaveTabWidget.qml \
    qml/TripTabWidget.qml \
    qml/CaveTreeDelegate.qml \
    qml/IconButton.qml \
    qml/GLTerrainRenderer.qml \
    shaders/simple.vert \
    shaders/simple.frag \
    shaders/LinePlot.vert \
    shaders/LinePlot.frag \
    shaders/NoteItem.vsh \
    shaders/NoteItem.frag \
    shaders/NoteItem.vert \
    shaders/tileVertex.vert \
    shaders/tileVertex.frag \
    shaders/tileVertex.geom \
    shaders/grid.vert \
    shaders/grid.frag \
    qml/FileDialog.qml \
    qml/ButtonGroup.qml \
    qml/Splitter.qml \
    qml/NoteStation.qml \
    qml/PanZoomInteraction.qml \
    qml/NoteItem.qml \
    qml/HelpBox.qml \
    qml/NoteTransformEditor.qml \
    qml/ScrapInteraction.qml \
    qml/NoteStationInteraction.qml \
    qml/NoteItemSelectionInteraction.qml \
    qml/NoteNorthInteraction.qml \
    qml/InteractionManager.qml \
    qml/NoteScaleInteraction.qml \
    qml/HelpArea.qml \
    qml/LabelWithHelp.qml \
    qml/LabelValueUnit.qml \
    qml/ClickTextInput.qml \
    qml/DoubleClickTextInput.qml \
    qml/Style.qml \
    qml/CoreClickTextInput.qml \
    qml/NoteScaleInput.qml \
    qml/Pallete.qml \
    qml/UnitInput.qml \
    qml/NoteNorthUpInput.qml \
    qml/GlobalShadowTextInput.qml \
    qml/DataBox.qml \
    qml/Utils.js \
    qml/CavewhereMainWindow.qml \
    shaders/scrap.vert \
    shaders/scrap.frag \
    installer/mac/installMac.sh \
    shaders/scrap.geom \
    qml/SurveyEditor.qml \
    qml/StationBox.qml \
    qml/ShotDistanceDataBox.qml \
    qml/ClinoReadBox.qml \
    qml/CompassReadBox.qml \
    qml/ReadingBox.qml \
    qml/Navigation.js \
    qml/ImageExplorer.qml \
    qml/NoteExplorer.qml \
    qml/NotesGallery.qml \
    qml/ImageArrowNavigation.qml \
    qml/FrontSightReadingBox.qml \
    qml/BackSightReadingBox.qml \
    qml/TeamTable.qml \
    qml/SectionLabel.qml \
    qml/BreakLine.qml \
    qml/RemoveButton.qml \
    qml/AddButton.qml \
    qml/CalibrationEditor.qml \
    qml/CheckableGroupBox.qml \
    qml/GroupBox.qml \
    qml/TapeCalibrationEditor.qml \
    qml/FrontSightCalibrationEditor.qml \
    qml/BackSightCalibrationEditor.qml \
    qml/DeclainationEditor.qml \
    qml/InformationButton.qml \
    qml/ErrorHelpArea.qml \
    qml/ErrorHelpBox.qml \
    qml/DataMainPage.qml \
    qml/ExportSurveyMenuItem.qml \
    qml/ShadowRectangle.qml \
    qml/TitleLabel.qml \
    qml/CaveDataToolbar.qml \
    qml/Menu.qml \
    qml/ContextMenu.qml \
    qml/MenuItem.qml \
    qml/GlobalMenuMouseHandler.qml \
    qml/Label3d.qml \
    qml/FileButtonAndMenu.qml \
    qml/ScrapOutlinePoint.qml \
    qml/PointItem.qml \
    qml/ScrapPointItem.qml \
    qml/ScrapPointMouseArea.qml \
    qml/FloatingGroupBox.qml \
    qml/NoteResolution.qml \
    qml/UnitValueInput.qml \
    qml/CaveOverviewPage.qml \
    qml/CaveLengthComponent.qml \
    qml/CaveLengthAndDepth.qml \
    qml/ContextMenuButton.qml \
    qml/RemoveDataRectangle.qml \
    qml/CameraSettings.qml \
    qml/ToggleSlider.qml \
    src/cavewhere.proto \
    src/qt.proto \
    docs/FileFormatDocumentation.txt \
    Cavewhere.rc \
    qml/RemoveAskBox.qml \
    shaders/compass/compass.vsh \
    shaders/compass/compass.fsh \
    shaders/compass/compassShadowX.vsh \
    shaders/compass/compassShadow.fsh \
    shaders/compass/compassShadowY.vsh \
    shaders/compass/compassShadowOutput.vsh \
    shaders/compass/compassShadowOutput.fsh \
    qml/ScaleBar.qml \
    qml/AboutWindow.qml \
    LICENSE.txt \
    qml/LicenseWindow.qml \
    qml/CavewhereLogo.qml \
    qml/LoadNotesWidget.qml \
    qml/LoadNotesIconButton.qml \
    IncludeQMath3d.pri \
    Info.plist \
    cavewhere.qbs \
    ProtoBuffer.qbs

RESOURCES += \
    resources.qrc

INCLUDEPATH += src src/utils src/serialization . .ui
DEPENDPATH += INCLUDEPATH

include(IncludeQMath3d.pri)

#For compiling Google ProtoBuffer
PROTO_FILES += \
    src/cavewhere.proto \
    src/qt.proto

unix {
    PROTOC = protoc
}

win32 {
    PROTO = C:/windowsBuild/libs/win32/protobuf-2.5.0rc1/vsprojects
    PROTOC = $${PROTO}/Release/protoc.exe
}

protoc.output = src/serialization/${QMAKE_FILE_BASE}.pb.cc
protoc.commands = $${PROTOC} --proto_path=src --cpp_out=src/serialization ${QMAKE_FILE_NAME}
protoc.input = PROTO_FILES
protoc.CONFIG = target_predeps
protoc.variable_out = SOURCES
QMAKE_EXTRA_COMPILERS += protoc

CavewhereVersion = $$system(git describe)
QMAKE_CXXFLAGS += -DCAVEWHERE_VERSION=\\\"$${CavewhereVersion}\\\"

macx {
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.7
    LIBS += -L/usr/local/lib -lboost_serialization-mt -lboost_wserialization-mt
    ICON = cavewhereIcon.icns
    INCLUDES += /usr/local/include
    QMAKE_INFO_PLIST = Info.plist
}

unix {
    LIBS += -lz -L/usr/lib -L/usr/local/lib -lsquish -lprotobuf
    QMAKE_LFLAGS += '-Wl,-rpath,\'/usr/local/lib\''
    INCLUDEPATH += /usr/local/include

#    INCLUDEPATH += \
#    /home/blitz/Qt5.0.0beta1/Desktop/Qt/5.0.0-beta1/gcc_64/include/QtOpenGL \
##    /home/blitz/Qt5.0.0beta1/Desktop/Qt/5.0.0-beta1/gcc_64/include/QtQML \
#    /home/blitz/Qt5.0.0beta1/Desktop/Qt/5.0.0-beta1/gcc_64/include/QtQuick \
#    /home/blitz/Qt5.0.0beta1/Desktop/Qt/5.0.0-beta1/gcc_64/include/QtCore \
#    /home/blitz/Qt5.0.0beta1/Desktop/Qt/5.0.0-beta1/gcc_64/include/QtGui \
#    /home/blitz/Qt5.0.0beta1/Desktop/Qt/5.0.0-beta1/gcc_64/include/QtConcurrent \
#    /home/blitz/Qt5.0.0beta1/Desktop/Qt/5.0.0-beta1/gcc_64/include/Qt3D \
#    /home/blitz/Qt5.0.0beta1/Desktop/Qt/5.0.0-beta1/gcc_64/include/QtSql \
#    /home/blitz/Qt5.0.0beta1/Desktop/Qt/5.0.0-beta1/gcc_64/include/QtXml \
#    /home/blitz/Qt5.0.0beta1/Desktop/Qt/5.0.0-beta1/gcc_64/include/QtXmlPatterns \
#    /home/blitz/Qt5.0.0beta1/Desktop/Qt/5.0.0-beta1/gcc_64/include/Qt \
#    /home/blitz/Qt5.0.0beta1/Desktop/Qt/5.0.0-beta1/gcc_64/include
}

linux {
    LIBS += -lboost_serialization -lboost_wserialization
}

win32 {
    SQUISH = C:/windowsBuild/libs/win32/squish-1.11/squish-1.11
    BOOST = c:/windowsBuild/libs/win32/boost_1_48_0/boost_1_48_0
    ZLIBSOURCE = C:/windowsBuild/libs/win32/zlib-1.2.5
    PROTO = C:/windowsBuild/libs/win32/protobuf-2.5.0rc1/vsprojects

    RC_FILE = Cavewhere.rc

    #This is so std::numerical_limits works
    DEFINES += NOMINMAX

    INCLUDEPATH += $${SQUISH}
    LIBS += -L$${SQUISH}/lib/vs9

    INCLUDEPATH += $${BOOST}
    LIBS += -L$${BOOST}/stage/lib

    INCLUDEPATH += $${ZLIBSOURCE}
    LIBS += -L$${ZLIBSOURCE}

    INCLUDEPATH += $${PROTO}/include

    CONFIG(debug, debug|release) {
        CONFIG += console
        LIBS += -L$${PROTO}/Debug/
        LIBS += -lzlibd -lsquishd -llibprotobuf

        message("Building Win32 in Debug")

        #Copy all the libraries
        DESTDIR_WIN = debug
        DESTDIR_WIN ~= s,/,\\,g
        QT_BIN_WIN = $$[QT_INSTALL_BINS]
        QT_BIN_WIN ~= s,/,\\,g
        ZLIBSOURCE_WIN = $${ZLIBSOURCE}
        ZLIBSOURCE_WIN ~= s,/,\\,g

        system($$quote(cmd /c copy /y $${ZLIBSOURCE_WIN}\\zlibd1.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
#        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\QtQuickd5.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
#        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\QtOpenGLd5.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
#        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\QtQMLd5.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
#        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\Qt3Dd5.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
#        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\QtWidgetsd5.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
#        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\QtXmld5.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
#        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\QtConcurrentd5.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
#        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\QtSqld5.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
#        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\QtGuid5.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
#        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\QtCored5.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
#        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\QtV8d5.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
#        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\QtNetworkd5.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
#        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\icuin49.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
#        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\icuuc49.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
#        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\icudt49.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
#        system($$quote(cmd /c copy /y c:\\windows\\system32\\MSVCP100D.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
#        system($$quote(cmd /c copy /y c:\\windows\\system32\\MSVCR100D.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
    }

    CONFIG(release, debug|release) {
        CONFIG += console
        LIBS += -L$${PROTO}/Release/
        LIBS += -lzlib -lsquish -llibprotobuf

        message("Building Win32 in Release")

        #Copy all the libraries
        DESTDIR_WIN = release
        DESTDIR_WIN ~= s,/,\\,g
        QT_BIN_WIN = $$[QT_INSTALL_BINS]
        QT_BIN_WIN ~= s,/,\\,g
        ZLIBSOURCE_WIN = $${ZLIBSOURCE}
        ZLIBSOURCE_WIN ~= s,/,\\,g

        system($$quote(cmd /c copy /y $${ZLIBSOURCE_WIN}\\zlib1.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\Qt5Quick.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\Qt5OpenGL.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\Qt5QML.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\Qt53D.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\Qt5Widgets.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\Qt5Xml.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\Qt5Concurrent.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\Qt5Sql.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\Qt5Gui.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\Qt5Core.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\Qt5V8.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\Qt5Network.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\Qt5Svg.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\icuin51.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\icuuc51.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\icudt51.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\libGLESv2.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\D3DCompiler_43.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
        system($$quote(cmd /c copy /y $${QT_BIN_WIN}\\libEGL.dll $${DESTDIR_WIN}$$escape_expand(\\n\\t)))
        system($$quote(cmd /c xcopy /y /E /I $${QT_BIN_WIN}\\..\\plugins\\platforms $${DESTDIR_WIN}\\platforms$$escape_expand(\\n\\t)))
        system($$quote(cmd /c xcopy /y /E /I $${QT_BIN_WIN}\\..\\plugins\\imageformats $${DESTDIR_WIN}\\imageformats$$escape_expand(\\n\\t)))
        system($$quote(cmd /c xcopy /y /E /I $${QT_BIN_WIN}\\..\\plugins\\sqldrivers $${DESTDIR_WIN}\\sqldrivers$$escape_expand(\\n\\t)))
        system($$quote(cmd /c xcopy /y /E /I $${QT_BIN_WIN}\\..\\qml\\QtQuick.2 $${DESTDIR_WIN}\\QtQuick.2$$escape_expand(\\n\\t)))
        system($$quote(cmd /c xcopy /y /E /I $${QT_BIN_WIN}\\..\\qml\\QtQuick\\Window.2 $${DESTDIR_WIN}\\QtQuick\\Window.2$$escape_expand(\\n\\t)))
        system($$quote(cmd /c xcopy /y /E /I $${QT_BIN_WIN}\\..\\qml\\QtQuick\\Controls $${DESTDIR_WIN}\\QtQuick\\Controls$$escape_expand(\\n\\t)))
        system($$quote(cmd /c xcopy /y /E /I $${QT_BIN_WIN}\\..\\qml\\QtQuick\\Dialogs $${DESTDIR_WIN}\\QtQuick\\Dialog$$escape_expand(\\n\\t)))
        system($$quote(cmd /c xcopy /y /E /I $${QT_BIN_WIN}\\..\\qml\\QtQuick\\Layouts $${DESTDIR_WIN}\\QtQuick\\Layouts$$escape_expand(\\n\\t)))
        system($$quote(cmd /c xcopy /y /E /I $${QT_BIN_WIN}\\..\\qml\\QtGraphicalEffects $${DESTDIR_WIN}\\QtGraphicalEffects$$escape_expand(\\n\\t)))
     }
}

cache()
