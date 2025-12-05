/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

QQ.Item {
    id: itemId

    property int scrapType
    property NoteTranformation noteTransform
    property HelpArea northUpHelp
    property alias enable: setNorthButton.visible

    signal northUpInteractionActivated()

    implicitHeight: row.height
    implicitWidth: row.width

    RowLayout {
        id: row

        RoundButton {
            id: setNorthButton
            objectName: "setNorthButton"
            radius: 2
            icon.source: {
                switch(itemId.scrapType) {
                case Scrap.Plan:
                    return "qrc:/icons/svg/north.svg"
                case Scrap.RunningProfile:
                    return "qrc:/icons/svg/up.svg"
                case Scrap.ProjectedProfile:
                    return "qrc:/icons/svg/up.svg"
                default:
                    return "qrc:/icons/svg/error.svg"
                }
            }

            onClicked: itemId.northUpInteractionActivated()
        }

        LabelWithHelp {
            id: labelId
            helpArea: itemId.northUpHelp
            text: {
                switch(itemId.scrapType) {
                case Scrap.Plan:
                    return "North"
                case Scrap.RunningProfile:
                    return "Up"
                case Scrap.ProjectedProfile:
                    return "Up"
                default:
                    return "Error"
                }
            }
        }

        ClickTextInput {
            id: clickInput
            readOnly: !itemId.enable
            text: ""
            onFinishedEditting: ( { } )
        }

        Text {
            id: unit
            textFormat: Text.RichText
            text: "&deg"
        }
    }

    states: [
        QQ.State {
            when: itemId.noteTransform !== null

            QQ.PropertyChanges {
                clickInput {
                    text: noteTransform.northUp.toFixed(2)
                    onFinishedEditting: noteTransform.northUp = newText
                }
            }
        }
    ]
}
