#-------------------------------------------------
#
# Project created by QtCreator 2010-08-19T19:44:25
#
#-------------------------------------------------

QT       += core gui declarative xml


TARGET = SurveyEditor
TEMPLATE = app


SOURCES += src/main.cpp \
src/cwSurveyEditorMainWindow.cpp \
    src/cwSurveyChunk.cpp \
    src/cwStation.cpp \
    src/cwShot.cpp \
    src/cwSurvexImporter.cpp \
    src/cwSurveyChuckView.cpp \
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
    src/cwTripCalibration.cpp

HEADERS  += src/cwSurveyEditorMainWindow.h \
    src/cwSurveyChunk.h \
    src/cwStation.h \
    src/cwShot.h \
    src/cwSurvexImporter.h \
    src/cwSurveyChuckView.h \
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
    src/cwTripCalibration.h


FORMS    += src/cwSurveyEditorMainWindow.ui \
    src/cwImportSurvexDialog.ui

OTHER_FILES += qml/DataBox.qml \
qml/Navigation.js \
qml/NavigationRectangle.qml \
qml/ReadingBox.qml \
qml/ShadowRectangle.qml \
qml/StationBox.qml \
qml/SurveyChunk.js \
qml/SurveyChunk.qml \
qml/TitleLabel.qml \
qml/DataBoxEntryState.qml \
qml/ClinoReadBox.qml \
 qml/CompassReadBox.qml \
    qml/DistanceDataBox.qml \
    qml/LeftDataBox.qml \
    qml/SurveyEditor.qml \
    qml/RightDataBox.qml \
    qml/UpDataBox.qml \
    qml/DownDataBox.qml \
    qml/ShotDistanceDataBox.qml \
    qml/FrontClinoReadBox.qml \
    qml/BackClinoReadBox.qml \
    qml/FrontCompassReadBox.qml \
    qml/BackCompassReadBox.qml \
    qml/SurveyChunkList.qml \
    qml/ScrollBar.qml \
    qml/NotesGallery.qml \
    qml/CavePage.qml

RESOURCES += \
    icons.qrc

INCLUDEPATH += src .
