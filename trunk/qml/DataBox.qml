import Qt 4.7
import "Navigation.js" as NavigationHandler

NavigationRectangle {
    id: dataBox

    property alias dataValue: dataTextInput.text
    property alias dataValidator: dataTextInput.validator
    property alias edittorOpen: edittor.visible
    property variant dataObject //For hooking up signals and slots in subclasses
    property int rowIndex: -1

    signal rightClicked(int index)
    signal splitOn(int index)

    border.color: "lightgray"
    border.width: 1

    color : "white"

    //Behavior on x { PropertyAnimation { duration: 100 } }
    Behavior on y { PropertyAnimation { duration: 250 } }
    Behavior on opacity  { PropertyAnimation { duration: 250 } }


    Rectangle {
        id: interalHighlight
        border.color: "black";
        anchors.fill: parent;
        anchors.margins: 1;
        border.width: 1;
        color: Qt.rgba(0, 0, 0, 0);
        visible: dataBox.focus || dataTextInput.focus
    }

    ShadowRectangle {
        id: edittor
        color: "white"
        anchors.centerIn: parent;
        width: dataTextInput.width > parent.width ? dataTextInput.width + 5 : parent.width + 5;
        height: parent.height + 5;
        visible: false;

        TextInput {
            id: dataTextInput
            anchors.centerIn: parent
            selectByMouse: true;         
        }
    }


    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPressed: {
            dataBox.focus = true;

            if(mouse.button == Qt.RightButton) {
                dataBox.rightClicked(rowIndex); //Emit signal
            }
        }

        onDoubleClicked: {
            dataTextInput.focus = true;
            var coords = mapToItem(dataTextInput, mouse.x, mouse.y);
            var cursorPosition = dataTextInput.positionAt(coords.x);
            dataTextInput.cursorPosition = cursorPosition;
            dataBox.state = 'MiddleTyping';
        }
    }

    Text {
        id: dataText
        anchors.centerIn: parent
        text: dataValue
    }

    Keys.onPressed: {
        console.log("Key has been pressed");

        NavigationHandler.HandleTabEvent(event, dataBox);
        NavigationHandler.HandleArrowEvent(event, dataBox);

        if(!event.accepted) {
            if(event.key > Qt.Key_Space && event.key <= Qt.Key_AsciiTilde) {
                var oldDataValue = dataValue;
                dataValue = dataValue + event.text;
                if(dataTextInput.acceptableInput) {
                    dataBox.state = 'EndTyping'
                    dataTextInput.focus = true
                } else {
                    dataValue = oldDataValue;
                }
            }

            if(event.key == Qt.Key_Backspace) {
                console.log("Back pressed")
                state = 'EndTyping';
                dataTextInput.focus = true;
                dataValue = dataValue.substring(0, dataValue.length - 1);
            }
        }
    }

    Keys.onSpacePressed: {
        console.log("Split on " + rowIndex);
        dataBox.splitOn(rowIndex); //Emit signal
    }

    Keys.onEnterPressed: {
        state = 'MiddleTyping';
        dataTextInput.focus = true;
        console.log("middelTyping enter");
    }

    Keys.onReturnPressed: {
        state = 'MiddleTyping';
        dataTextInput.focus = true;
         console.log("middelTyping return");
    }

    Keys.onDeletePressed: {
        console.log("Delete pressed")
        state = 'EndTyping';
        dataValue = '';
        dataTextInput.focus = true;
    }



    states: [

        State {
            name: "MiddleTyping"
            PropertyChanges {
                target: edittor
                visible: true
            }

            PropertyChanges {
                target: dataText
                visible: false
            }

            PropertyChanges {
                target: dataTextInput
                Keys.onPressed: {
                    NavigationHandler.HandleTabEvent(event, dataBox);
                    NavigationHandler.EnterNavigation(event, dataBox);
                    if(event.accepted) {
                        dataBox.state = ''; //Default state
                    }
                }

                onFocusChanged: {
                    if(!focus) {
                        dataBox.state = '';
                    }
                }
            }

            PropertyChanges {
                target: dataBox
                z: 1
            }

        },

        State {
            name: "EndTyping"
            extend: "MiddleTyping"
            PropertyChanges {
                target: edittor
                visible: true
            }

            PropertyChanges {
                target: dataText
                visible: false
            }

            PropertyChanges {
                target: dataTextInput

                Keys.onPressed: {
                    NavigationHandler.HandleTabEvent(event, dataBox);
                    NavigationHandler.HandleArrowEvent(event, dataBox);
                    NavigationHandler.EnterNavigation(event, dataBox);
                    if(event.accepted) {
                        dataBox.state = ''; //Default state
                    }
                }//    for(int i = 0; i < StationRows.size(); i++) {
                //        StationRow& row = StationRows[i];
                //        foreach(QDeclarativeItem* item, row.items()) {
                //            qDebug() << "Deleting:" << item;
                //            delete item;
                //        }
                //    }

                onFocusChanged: {
                    if(!focus) {
                        dataBox.state = '';
                    }
                }

                cursorPosition: text.length
            }

            PropertyChanges {
                target: dataBox
                z: 1
            }
        }
    ]


}
