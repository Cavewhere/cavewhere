#-------------------------------------------------
#
# Project created by QtCreator 2010-08-19T19:44:25
#
#-------------------------------------------------

QT       += core gui declarative


TARGET = SurveyEditor
TEMPLATE = app


SOURCES += src/main.cpp \
src/cwSurveyEditorMainWindow.cpp \
    src/cwSurveyChunk.cpp \
    src/cwStation.cpp \
    src/cwShot.cpp \
    src/cwStationModel.cpp \
    src/cwSurvexImporter.cpp \
    src/cwSurveyChuckView.cpp \
    src/cwSurveyChunkGroupView.cpp \
    src/cwClinoValidator.cpp \
    src/cwStationValidator.cpp \
    src/cwValidator.cpp \
    src/cwCompassValidator.cpp \
    src/cwDistanceValidator.cpp \
    src/cwSurvexExporter.cpp \
    src/cwImageModel.cpp \
    src/cwSurveyNoteModel.cpp \
    src/cwNote.cpp \
    src/cwNoteItem.cpp \
    src/cwSurveyTrip.cpp \
    src/cwCave.cpp \
    src/cwCavingRegion.cpp \
    src/cwRegionTreeModel.cpp \
    src/cwSurvexGlobalData.cpp \
    src/cwSurvexBlockData.cpp \
    src/cwSurvexImporterModel.cpp \
    src/cwImportSurvexDialog.cpp \
    src/cwGlobalIcons.cpp

HEADERS  += src/cwSurveyEditorMainWindow.h \
    src/cwSurveyChunk.h \
    src/cwStation.h \
    src/cwShot.h \
    src/cwStationModel.h \
    src/cwSurvexImporter.h \
    src/cwSurveyChuckView.h \
    src/cwSurveyChunkGroupView.h \
    src/cwClinoValidator.h \
    src/cwStationValidator.h \
    src/cwValidator.h \
    src/cwCompassValidator.h \
    src/cwDistanceValidator.h \
    src/cwSurvexExporter.h \
    src/cwImageModel.h \
    src/cwSurveyNoteModel.h \
    src/cwNote.h \
    src/cwNoteItem.h \
    src/cwSurveyTrip.h \
    src/cwCave.h \
    src/cwCavingRegion.h \
    src/cwRegionTreeModel.h \
    src/cwSurvexGlobalData.h \
    src/cwSurvexBlockData.h \
    src/cwSurvexImporterModel.h \
    src/cwImportSurvexDialog.h \
    src/cwGlobalIcons.h


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
    qml/NotesGallery.qml

RESOURCES += \
    icons.qrc

INCLUDEPATH += src .
