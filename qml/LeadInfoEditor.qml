import QtQuick 2.0
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.2
import Cavewhere 1.0

FloatingGroupBox {
    id: groupBox

    property ScrapLeadView leadView;

    function updateData(begin, end, roles) {
        if(leadView.selectedItemIndex >= begin && leadView.selectedItemIndex <= end) {
            for(var roleIndex in roles) {
                var role = roles[roleIndex]
                switch(role) {
                case Scrap.LeadDesciption:
                    leadDescriptionArea.text = leadView.scrap.leadData(role, leadView.selectedItemIndex);
                    break;
                case Scrap.LeadSize:
                    var size = leadView.scrap.leadData(role, leadView.selectedItemIndex);
                    widthTextId.text = (size.width >= 0 ? size.width : "?")
                    heightTextId.text = (size.height >= 0 ? size.height : "?")
                    break;
                case Scrap.LeadSupportedUnits:
                    unit.unitModel = leadView.scrap.leadData(role, leadView.selectedItemIndex);
                    break;
                case Scrap.LeadUnits:
                    unit.unit = leadView.scrap.leadData(role, leadView.selectedItemIndex);
                    break;
                default:
                    break;
                }
            }
        }
    }

    function toLeadDim(text) {
        if(text === "?") {
            return -1;
        } else if(text < 0) {
            return -1;
        }
        return text;
    }

    title: "Lead Info"
    visible: leadView !== null && leadView.selectedItemIndex >= 0

    ColumnLayout {

        //This connection update data in the editor
        Connections {
            target: leadView !== null ? leadView.scrap : null
            onLeadsDataChanged: updateData(begin, end, roles);
            ignoreUnknownSignals: true
        }

        Connections {
            target: leadView
            onSelectedItemIndexChanged: updateData(leadView.selectedItemIndex,
                                                   leadView.selectedItemIndex,
                                                   [Scrap.LeadSupportedUnits,
                                                    Scrap.LeadUnits,
                                                    Scrap.LeadSize,
                                                    Scrap.LeadDesciption])
            ignoreUnknownSignals: true
        }


        RowLayout {
            Text {
                text: "Size:"
            }

            TitledRectangle {
                title: "Width"

                ClickTextInput {
                    id: widthTextId
                    Layout.alignment: Qt.AlignHCenter
                    KeyNavigation.tab: heightTextId
                    onFinishedEditting: {
                        var oldSize = leadView.scrap.leadData(Scrap.LeadSize, leadView.selectedItemIndex);
                        leadView.scrap.setLeadData(Scrap.LeadSize,
                                                   leadView.selectedItemIndex,
                                                   Qt.size(toLeadDim(newText),
                                                           oldSize.height))
                    }

                }
            }

            Text {
                text: "x"
            }

            TitledRectangle {
                title: "Height"

                ClickTextInput {
                    id: heightTextId
                    KeyNavigation.tab: leadDescriptionArea
                    KeyNavigation.backtab: widthTextId
                    Layout.alignment: Qt.AlignHCenter
                    onFinishedEditting: {
                        var oldSize = leadView.scrap.leadData(Scrap.LeadSize, leadView.selectedItemIndex);
                        leadView.scrap.setLeadData(Scrap.LeadSize,
                                                   leadView.selectedItemIndex,
                                                   Qt.size(oldSize.width,
                                                           toLeadDim(newText)))
                    }
                }
            }

            UnitInput {
                id: unit
                readOnly: true
            }
        }

        TextArea {
            id: leadDescriptionArea
            KeyNavigation.backtab: heightTextId
            implicitHeight: 100
            onTextChanged: {
                if(leadView !== null && leadView.scrap !== null) {
                    leadView.scrap.setLeadData(Scrap.LeadDesciption, leadView.selectedItemIndex, text)
                }
            }
        }

        LeadQuickComments {
            Layout.fillWidth: true
            category: "Passage Type"
            keywords: ["Aven", "Borehole", "Caynon", "Dig", "Dome", "Phreatic", "Pit", "Sump",]
            textArea: leadDescriptionArea
        }

        LeadQuickComments {
            category: "Passage Fill"
            Layout.fillWidth: true
            keywords: ["Breakdown", "Cobbles", "Muddy", "Sandy", "Water"]
            textArea: leadDescriptionArea
        }

        LeadQuickComments {
            category: "Passage Traversal"
            Layout.fillWidth: true
            keywords: ["Crawl", "Climb", "Low", "High", "Swim", "Tight", "Traverse", "Walking"]
        }

        LeadQuickComments {
            Layout.fillWidth: true
            category: "Stream Type"
            keywords: ["Large", "Stream", "Small", "Trickle"]
            textArea: leadDescriptionArea
        }

        LeadQuickComments {
            Layout.fillWidth: true
            category: "Air"
            keywords: ["Strong", "Air", "Weak"]
            textArea: leadDescriptionArea
        }

        LeadQuickComments {
            Layout.fillWidth: true
            category: "Gear Needed"
            keywords: ["Bolts", "Cable Ladder", "Handline", "Hammer", "Rope"]
            textArea: leadDescriptionArea
        }
    }
}

