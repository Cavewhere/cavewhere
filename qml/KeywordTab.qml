import QtQuick 2.0
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.12
import Cavewhere 1.0

Item {

    KeywordItemFilterModel {
        id: filterModel
        keywordModel: rootData.keywordItemModel
        lastKey: comboBoxId.currentText
    }

    ColumnLayout {
        anchors.fill: parent

        ComboBox {
            id: comboBoxId
            model: filterModel.possibleKeys
        }

        ListView {
            model: filterModel

            Layout.fillHeight: true
            Layout.fillWidth: true

            delegate: RowLayout {
                CheckBox {
                    onCheckedChanged: {
                        for(var i in objectsRole) {
                            var obj = objectsRole[i];
                            console.log("Changing visiblity of " + obj)
                            obj.enabled = checked
                        }
                    }
                }

                Text {
                    text: valueRole
                }

                Text {
                    text: "(" + objectCountRole + ")"
                }
            }
        }
    }
}
