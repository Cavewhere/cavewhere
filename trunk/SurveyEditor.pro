#-------------------------------------------------
#
# Project created by QtCreator 2010-08-19T19:44:25
#
#-------------------------------------------------

QT       += core gui declarative xml opengl sql

TARGET = SurveyEditor
TEMPLATE = app

# Include JS debugger library if QMLJSDEBUGGER_PATH is set
!isEmpty(QMLJSDEBUGGER_PATH) {
    include($$QMLJSDEBUGGER_PATH/qmljsdebugger-lib.pri)
} else {
    DEFINES -= QMLJSDEBUGGER
}

SOURCES += src/main.cpp \
    src/cwSurveyEditorMainWindow.cpp \
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
    src/cwStationReference.cpp \
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
    src/cwGraphicsSceneMouseTransition.cpp \
    src/cwLinePlotGeometryTask.cpp \
    src/cwGLLinePlot.cpp \
    src/cwCollisionRectKdTree.cpp \
    src/cw3dRegionViewer.cpp \
    src/cwSaveXMLProjectTask.cpp \
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
    src/cwWheelArea.cpp

HEADERS  += src/cwSurveyEditorMainWindow.h \
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
    src/cwStationReference.h \
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
    src/cwGraphicsSceneMouseTransition.h \
    src/cwLinePlotGeometryTask.h \
    src/cwGLLinePlot.h \
    src/cwCollisionRectKdTree.h \
    src/cw3dRegionViewer.h \
    src/cwSaveXMLProjectTask.h \
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
    src/cwWheelArea.h



FORMS    += src/cwSurveyEditorMainWindow.ui \
    src/cwImportSurvexDialog.ui \
    src/cwTaskProgressDialog.ui

OTHER_FILES += qml/*.qml \
qml/*.js \
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
    qml/DoubleClickTextInput.qml \
    qml/GLTerrainRenderer.qml \
shaders/*.frag \
shaders/*.vert \
shaders/*.geom \
    shaders/simple.vert \
    shaders/simple.frag \
    shaders/LinePlot.vert \
    shaders/LinePlot.frag \
    shaders/NoteItem.vsh \
    shaders/NoteItem.frag \
    shaders/NoteItem.vert \
    qml/FileDialog.qml \
    qml/ButtonGroup.qml \
    qml/Splitter.qml \
    qml/NoteStation.qml \
    qml/Positioner3D.qml \
    qml/PanZoomInteraction.qml \
    qml/NoteItem.qml \
    qml/AddScrapInteraction.qml

RESOURCES += \
    icons.qrc

INCLUDEPATH += src src/utils . /usr/local/include /opt/local/include
DEPENDPATH += INCLUDEPATH

LIBS += -lz -lGLEW -L/usr/local/lib -L/opt/local/lib -lsquish -lboost_serialization
