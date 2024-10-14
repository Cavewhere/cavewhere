import QtQuick 2.0 as QQ
import cavewherelib

ScrapPointItem {
    id: scrapPointId

    width: 2
    height: 2

    SelectedBackground {
        id: selectedBackground

        anchors.fill: questionImage

        visible: scrapPointId.selected && scrapPointId.scrapItem.selected
    }

    QQ.Image {
        id: questionImage
        source: "qrc:/icons/question.png"
        anchors.centerIn: parent

        ScrapPointMouseArea {
            id: stationMouseArea
            anchors.fill: parent

            onPointSelected: scrapPointId.select();
            onPointMoved: (noteCoord) =>
                          scrapPointId.scrap.setLeadData(Scrap.LeadPositionOnNote,
                                                         scrapPointId.pointIndex,
                                                         Qt.point(noteCoord.x, noteCoord.y));
        }
    }

    QQ.Keys.onDeletePressed: {
        scrap.removeLead(pointIndex);
    }

    QQ.Keys.onPressed: (event) => {
        if(event.key === Qt.Key_Backspace) {
            scrap.removeLead(pointIndex);
        }
    }
}

