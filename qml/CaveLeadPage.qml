import QtQuick 2.0
import QtQuick.Controls 1.2
import Cavewhere 1.0

Rectangle {
    id: leadPage

    property Cave cave;

    LeadModel {
        id: leadModel
        regionModel: rootData.regionTreeModel
        cave: leadPage.cave
    }

    TableView {
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        width: 400
        model: leadModel;

        TableViewColumn { role: "leadCompleted"; width: 20 }
        TableViewColumn { role: "leadNearestStation"; title: "Station"; width: 50}
        TableViewColumn { role: "leadSizeAsString"; title: "Size"; width: 50 }
        TableViewColumn { role: "leadDescription"; title: "Description" }
        TableViewColumn { role: "leadPosition"; title: "Goto" }


        itemDelegate: Item {
            Loader {
                sourceComponent: {
                    switch(styleData.column) {
                    case 0:
                        return checkboxComponent;
                    case 1:
                    case 2:
                    case 3:
                        return textComponent
                    case 4:
                        return gotoViewComponent
                    }
                }
            }

            Component {
                id: checkboxComponent
                CheckBox {
                    property bool modelChecked: styleData.value
                    checked: modelChecked

                    onModelCheckedChanged: {
                        checked = modelChecked
                    }

                    onCheckedChanged: {
                        var index = leadModel.index(styleData.row);
                        leadModel.setData(index, checked, LeadModel.LeadCompleted);
                    }
                }
            }

            Component {
                id: textComponent
                Text {
                    text: styleData.value
                    x: 3
                }
            }

            Component {
                id: gotoViewComponent
                LinkText {
                    text: "Goto"
                    onClicked: {
                        var index = leadModel.index(styleData.row);
                        var scrap = leadModel.data(index, LeadModel.LeadScrap);
                        var leadIndex = leadModel.data(index, LeadModel.LeadIndexInScrap);

                        //Change to the view page, animate to the lead position, and select it
                        rootData.pageSelectionModel.currentPageAddress = "View"
                        pageView.currentPageItem.turnTableInteraction.centerOn(styleData.value, true);
                        pageView.currentPageItem.leadView.select(scrap, leadIndex);
                    }
                }
            }
        }
    }
}

