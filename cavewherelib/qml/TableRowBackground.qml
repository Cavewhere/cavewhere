import QtQuick

Rectangle {

    required property bool isSelected;
    required property int rowIndex;

    anchors.fill: parent

    color: {
        if(isSelected) {
            //Selected
            return "#d6e6ff"
        } else {
            //Alternating color background
            if(rowIndex % 2 == 1) {
                return "#f2f2f2"
            } else {
                return "white"
            }
        }
    }
}
