#-------------------------------------------------
#
# Project created by QtCreator 2010-08-19T19:44:25
#
#-------------------------------------------------


QT       += core gui declarative xml opengl sql

TARGET = Cavewhere
TEMPLATE = app

# Include JS debugger library if QMLJSDEBUGGER_PATH is set
!isEmpty(QMLJSDEBUGGER_PATH) {
    include($$QMLJSDEBUGGER_PATH/qmljsdebugger-lib.pri)
} else {
    DEFINES -= QMLJSDEBUGGER
}

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
    src/cwNoteItem.cpp \
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
    src/cwTreeView.cpp \
    src/cwQMLWidget.cpp \
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
    src/cwImageDatabase.cpp \
    src/cwImage.cpp \
    src/cwImageData.cpp \
    src/cwProject.cpp \
    src/cwAddImageTask.cpp \
    src/cwFileDialogHelper.cpp \
    src/cwGunZipReader.cpp \
    src/cwProjectImageProvider.cpp \
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
    src/cwWheelArea.cpp \
    src/cwNoteInteraction.cpp \
    src/cwScrapView.cpp \
    src/cwScrapStationView.cpp \
    src/cwLength.cpp \
    src/cwGlobals.cpp \
    src/cwInteraction.cpp \
    src/cwQMLRegister.cpp \
    src/cwNorthArrowItem.cpp \
    src/cwPositioner3D.cpp \
    src/cwMainWindow.cpp \
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
    src/cwItemSelectionModel.cpp

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
    src/cwNoteItem.h \
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
    src/cwTreeView.h \
    src/cwQMLWidget.h \
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
    src/cwImageDatabase.h \
    src/cwImage.h \
    src/cwImageData.h \
    src/cwProject.h \
    src/cwAddImageTask.h \
    src/cwFileDialogHelper.h \
    src/cwGunZipReader.h \
    src/cwProjectImageProvider.h \
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
    src/cwWheelArea.h \
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
    src/cwMainWindow.h \
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
    src/cwItemSelectionModel.h


FORMS    += src/cwMainWindow.ui \
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
    qml/LengthInput.qml \
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
    qml/DataMainPage.qml #\
#    qml/CaveDataToolbar.qml

RESOURCES += \
    icons.qrc

INCLUDEPATH += src src/utils . .ui
DEPENDPATH += INCLUDEPATH

unix {
    INCLUDEPATH += /usr/local/include /opt/local/include
    LIBS += -lz -lGLEW -L/usr/lib -L/usr/local/lib -L/opt/local/lib -lsquish -lboost_serialization -lboost_wserialization
    QMAKE_LFLAGS += '-Wl,-rpath,\'/usr/local/lib\''
}

macx {
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.6
}

win32 {
    GLEW = C:/windowsBuild/libs/win32/glew-1.7.0-win32/glew-1.7.0
    SQUISH = C:/windowsBuild/libs/win32/squish-1.11/squish-1.11
    BOOST = c:/windowsBuild/libs/win32/boost_1_48_0/boost_1_48_0
    ZLIBSOURCE = C:/windowsBuild/libs/win32/zlib-1.2.5
    ZLIBDLL = C:/windowsBuild/libs/win32/zlib125dll

    #This is so std::numerical_limits works
    DEFINES += NOMINMAX

    #Statically link glew
    INCLUDEPATH += "$${GLEW}/include"
    LIBS += -L"$${GLEW}/lib" -lglew32

    INCLUDEPATH += "$${SQUISH}"
    LIBS += -L"$${SQUISH}/lib/vs9" -lsquishd

    INCLUDEPATH += "$${BOOST}"
    LIBS += -L"$${BOOST}/stage/lib"

    INCLUDEPATH += "$${ZLIBSOURCE}"
    LIBS += -L"$${ZLIBDLL}/static32" -lzlibstat

#    INCLUDEPATH += C:/Users/saraf/cavewhere/glew-1.7.0/include
#    LIBS += -LC:/Us_ers/saraf/cavewhere/glew-1.7.0/lib -lglew32

#    INCLUDEPATH += "C:/Program Files (x86)/GnuWin32/include"
#    LIBS += -L"C:/Program Files (x86)/GnuWin32/lib" -lzlib

#    INCLUDEPATH += "C:/Users/saraf/cavewhere/trunk/squish-1.11"
#    LIBS += -L"C:/Users/saraf/cavewhere/trunk/squish-1.11/lib/vs9" -lsquish

#    INCLUDEPATH += "C:/Users/saraf/cavewhere/sqlite-amalgamation-3070900"

#    INCLUDEPATH += "C:/Users/saraf/cavewhere/boost_1_48_0/boost_1_48_0"
#    LIBS += -L"C:\Users\saraf\cavewhere\boost_1_48_0\boost_1_48_0\bin.v2\libs\serialization\build\gcc-mingw-4.5.3\release\link-static\threading-multi" -lboost_serialization
}




























































