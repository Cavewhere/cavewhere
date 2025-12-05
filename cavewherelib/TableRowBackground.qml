import QtQuick as QQ
import cavewherelib

QQ.Rectangle {

    required property bool isSelected;
    required property int rowIndex;

    color: {
        if(isSelected) {
            //Selected
            return Theme.highlight
        } else {
            //Alternating color background
            if(rowIndex % 2 == 1) {
                return Theme.surfaceMuted
            } else {
                return Theme.surface
            }
        }
    }
}
