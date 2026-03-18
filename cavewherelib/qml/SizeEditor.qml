import QtQuick as QQ
import QtQuick.Layouts
import QtQuick.Controls as QC
import cavewherelib
RowLayout {
    id: editor

    property alias widthText: widthTextId.text
    property alias heightText: heightTextId.text
    property alias unit: unitId.unit
    property alias unitModel: unitId.unitModel
    property alias readOnly: widthTextId.readOnly
    property QQ.Item nextTab: null
    property alias widthTextObject: widthTextId
    property alias heightTextObject: heightTextId;

    signal widthFinishedEditting(string newText)
    signal heightFinishedEditting(string newText)

    DistanceValidator {
        id: distanceValidator
    }

    TitledRectangle {
        id: rect1
        title: "Width"

        ClickTextInput {
            id: widthTextId
            objectName: "widthText"
            Layout.alignment: Qt.AlignHCenter
            QQ.KeyNavigation.tab: heightTextId
            validator: distanceValidator
            onFinishedEditting: (newText) => editor.widthFinishedEditting(newText)
        }
    }

    Text {
        text: "x"
    }

    TitledRectangle {
        title: "Height"

        ClickTextInput {
            id: heightTextId
            objectName: "heightText"
            readOnly: editor.readOnly
            QQ.KeyNavigation.tab: editor.nextTab
            QQ.KeyNavigation.backtab: widthTextId
            Layout.alignment: Qt.AlignHCenter
            validator: distanceValidator
            onFinishedEditting: (newText) => editor.heightFinishedEditting(newText)
        }
    }

    UnitInput {
        id: unitId
        readOnly: true
    }
}
