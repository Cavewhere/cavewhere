import QtQuick
import QtQuick.Shapes
import QtQuick.Controls as QC
import QtQuick.Layouts
import CaveWhereSketch
import cavewherelib

StandardPage {

    SurveyEditor {
        anchors.fill: parent
        currentTrip: RootDataSketch.currentTrip
    }

}
