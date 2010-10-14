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
    src/cwSurveyImporter.cpp \
    src/cwSurveryChunkGroup.cpp \
    src/cwSurveyChuckView.cpp

HEADERS  += src/cwSurveyEditorMainWindow.h \
    src/cwSurveyChunk.h \
    src/cwStation.h \
    src/cwShot.h \
    src/cwStationModel.h \
    src/cwSurveyImporter.h \
    src/cwSurveryChunkGroup.h \
    src/cwSurveyChuckView.h


FORMS    += src/cwSurveyEditorMainWindow.ui

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
    qml/ScrollBar.qml

RESOURCES +=
