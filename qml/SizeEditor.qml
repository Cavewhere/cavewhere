import QtQuick 2.0 as QQ
import QtQuick.Layouts 1.1

RowLayout {
    id: editor

    property alias widthText: widthTextId.text
    property alias heightText: heightTextId.text
    property alias unit: unitId.unit
    property alias unitModel: unitId.unitModel
    property alias readOnly: widthTextId.readOnly
    property alias backgroundColor: rect1.color
    property QQ.Item nextTab: null
    property alias widthTextObject: widthTextId
    property alias heightTextObject: heightTextId;


    signal widthFinishedEditting(string newText)
    signal heightFinishedEditting(string newText)

    TitledRectangle {
        id: rect1
        title: "Width"

        ClickTextInput {
            id: widthTextId
            Layout.alignment: Qt.AlignHCenter
            QQ.KeyNavigation.tab: heightTextId
            onFinishedEditting: widthFinishedEditting(newText)
        }
    }

    Text {
        text: "x"
    }

    TitledRectangle {
        color: backgroundColor
        title: "Height"

        ClickTextInput {
            id: heightTextId
            readOnly: editor.readOnly
            QQ.KeyNavigation.tab: nextTab
            QQ.KeyNavigation.backtab: widthTextId
            Layout.alignment: Qt.AlignHCenter
            onFinishedEditting: heightFinishedEditting(newText)
        }
    }

    UnitInput {
        id: unitId
        readOnly: true
    }
}
