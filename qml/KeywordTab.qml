import QtQuick 2.0
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.15
import Cavewhere 1.0

Item {
    id: keywordTabId

    KeywordItemFilterModel {
        id: filterModel
        keywordModel: rootData.keywordItemModel
        lastKey: comboBoxId.currentText
    }

    ColumnLayout {
        anchors.fill: parent

        Repeater {
            model: filterModel.filterKeywords
            delegate: RowLayout {
                Layout.fillHeight: true
                Layout.fillWidth: true 

                RoundButton {
                    id: xButtonId
                    icon.source: "qrc:///icons/x.png"
                    icon.width: 32
                    icon.height: 32
                    onClicked: {
                        filterModel.filterKeywords.remove(index)
                    }
                }

                Text {
                    text: keyRole + " = " + valueRole
                }
            }
        }

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
                    checked: Qt.Checked
//                    {

//                        var visible = 0;
//                        var hidden = 0;

//                        for(var i in objectsRole) {
//                            var obj = objectsRole[i]
//                            if('visible' in obj) {
//                                if(obj.visible) {
//                                    visible++
//                                } else {
//                                    hidden++
//                                }
//                            } else if('enabled' in obj) {
//                                if(obj.enabled) {
//                                    visible++
//                                } else {
//                                    hidden++;
//                                }
//                            }

//                            if(visible > 0 && hidden > 0) {
//                                return Qt.PartiallyChecked
//                            }
//                        }

//                        if(visible > 0) {
//                            return Qt.Checked
//                        } else {
//                            return Qt.Unchecked
//                        }
//                    }

                    onCheckedChanged: {
                        for(var i in objectsRole) {
                            var obj = objectsRole[i];
                            console.log("Changing visiblity of " + obj)
                            if('enabled' in obj) {
                                obj.enabled = checked
                            }

                            if('visible' in obj) {
                                obj.visible = checked
                            }
                        }
                    }
                }

                RoundButton {
                    width: 32
                    height: 32

                    visible: objectCountRole > 1

                    icon.source: "qrc:///icons/rightCircleArrow-32x32.png"
                    icon.width: 32
                    icon.height: 32

                    onClicked: {
                        filterModel.addKeywordFromLastKey(valueRole);
                    }
                }

                Text {
                    text: valueRole + " (" + objectCountRole + ")"
                }

            }
        }
    }
}
