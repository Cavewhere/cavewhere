import QtQuick as QQ
import QtQuick.Layouts

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
            objectName: "widthText"
            Layout.alignment: Qt.AlignHCenter
            QQ.KeyNavigation.tab: heightTextId
            onFinishedEditting: (newText) => editor.widthFinishedEditting(newText)
        }
    }

    Text {
        text: "x"
    }

    TitledRectangle {
        color: editor.backgroundColor
        title: "Height"

        ClickTextInput {
            id: heightTextId
            objectName: "heightText"
            readOnly: editor.readOnly
            QQ.KeyNavigation.tab: editor.nextTab
            QQ.KeyNavigation.backtab: widthTextId
            Layout.alignment: Qt.AlignHCenter
            onFinishedEditting: (newText) => editor.heightFinishedEditting(newText)
        }
    }

    UnitInput {
        id: unitId
        readOnly: true
    }
}
