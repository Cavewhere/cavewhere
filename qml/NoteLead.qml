import QtQuick 2.0
import Cavewhere 1.0

ScrapPointItem {
    width: 2
    height: 2

    SelectedBackground {
        id: selectedBackground

        anchors.fill: questionImage

        visible: selected && scrapItem.selected
    }

    Image {
        id: questionImage
        source: "qrc:/icons/question.png"
        anchors.centerIn: parent

        ScrapPointMouseArea {
            id: stationMouseArea
            anchors.fill: parent

            onPointSelected: select();
            onPointMoved: scrap.setLeadData(Scrap.LeadPositionOnNote,
                                            pointIndex,
                                            Qt.point(noteCoord.x, noteCoord.y));
        }
    }

    Keys.onDeletePressed: {
        scrap.removeLead(pointIndex);
    }

    Keys.onPressed: {
        if(event.key === Qt.Key_Backspace) {
            scrap.removeLead(pointIndex);
        }
    }
}

