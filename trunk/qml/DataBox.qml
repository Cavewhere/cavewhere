import Qt 4.7
import Cavewhere 1.0
import "Navigation.js" as NavigationHandler

NavigationRectangle {
    id: dataBox

    property alias dataValue: editor.text
//    property alias editorOpen: edittor.visible
    property alias dataValidator: editor.validator
    property variant surveyChunk; //For hooking up signals and slots in subclasses
    property int rowIndex: -1
    property int dataRole

    signal rightClicked(int index)
    signal splitOn(int index)

    border.color: "lightgray"
    border.width: 1

    color : "white"

    //This causes memory leaks in qt 4.7.1!!!
//    Behavior on y { PropertyAnimation { duration: 250 } }
//    Behavior on opacity  { PropertyAnimation { duration: 250 } }

//    onDataValidatorChanged: {
//        dataTextInput.validator = dataValidator;
//    }

    Rectangle {
        id: interalHighlight
        border.color: "black";
        anchors.fill: parent;
        anchors.margins: 1;
        border.width: 1;
        color: Qt.rgba(0, 0, 0, 0);
        visible: dataBox.focus || editor.isEditting
    }

    ClickTextInput {
        id: editor
        anchors.fill: parent

//        onVisibleChanged: {
//            console.log("Visible changed!");
//        }
    }

//    ShadowRectangle {
//        id: edittor
//        color: "white"
//        anchors.centerIn: parent;
//        width: dataTextInput.width > parent.width ? dataTextInput.width + 5 : parent.width + 5;
//        height: parent.height + 5;
//        visible: false;

//        TextInput {
//            id: dataTextInput
//           //validator: dataValidator != null ? dataValidator : null;
//            anchors.centerIn: parent
//            selectByMouse: true;
//        }
//    }



//    MouseArea {
//        anchors.fill: parent
//        acceptedButtons: Qt.LeftButton | Qt.RightButton
//        onPressed: {
//            dataBox.focus = true;

//            if(mouse.button == Qt.RightButton) {
//                dataBox.rightClicked(rowIndex); //Emit signal
//            }
//        }

//        onDoubleClicked: {
//            dataTextInput.focus = true;
//            var coords = mapToItem(dataTextInput, mouse.x, mouse.y);
//            var cursorPosition = dataTextInput.positionAt(coords.x);
//            dataTextInput.cursorPosition = cursorPosition;
//            dataBox.state = 'MiddleTyping';
//        }
//    }

//    Text {
//        id: dataText
//        anchors.centerIn: parent
//        text: dataValue
//    }

//    Keys.forwardTo: [dataTextInput]
//    Keys.priority: Keys.AfterItem

    Keys.onPressed: {
        //console.log("Key has been pressed");

        NavigationHandler.HandleTabEvent(event, dataBox);
        NavigationHandler.HandleArrowEvent(event, dataBox);

        if(!event.accepted) {
            if(event.key == Qt.Key_Backspace) {
                //console.log("Back pressed")
                state = 'EndTyping';
                dataTextInput.focus = true;
                dataValue = dataValue.substring(0, dataValue.length - 1);
                return;
            }

            //dataBox.state = 'EndTyping';

            //dataTextInput.Keys.onPressed(event);

            //var oldDataValue = dataValue;
            if(dataTextInput.validator.validate(event.text) > 0 && event.text.length > 0) {
                dataBox.state = 'EndTyping'
                dataTextInput.focus = true
                dataValue = event.text
            } //else {
                //dataValue = oldDataValue;
            //}
        }
    }

    Keys.onSpacePressed: {
        dataBox.splitOn(rowIndex); //Emit signal
    }

    Keys.onEnterPressed: {
        state = 'MiddleTyping';
        dataTextInput.focus = true;
    }

    Keys.onReturnPressed: {
        state = 'MiddleTyping';
        dataTextInput.focus = true;
    }

    Keys.onDeletePressed: {
        //console.log("Delete pressed")
        state = 'EndTyping';
        dataValue = '';
        dataTextInput.focus = true;
    }

    onDataValueChanged: {
        surveyChunk.setData(dataRole, rowIndex, dataValue);
    }

    states: [

        State {
            name: "MiddleTyping"

//            PropertyChanges {
//                target: edittor
//                visible: true
//            }


//            PropertyChanges {
//                target: dataText
//                visible: false
//            }

            PropertyChanges {
                target: dataTextInput
                Keys.onPressed: {
                    NavigationHandler.HandleTabEvent(event, dataBox);
                    NavigationHandler.EnterNavigation(event, dataBox);
                    if(event.accepted) {
                        dataBox.state = ''; //Default state
                    }
                }

                Keys.onEscapePressed: {
                    dataBox.state = ''; //Default state
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

//            PropertyChanges {
//                target: edittor
//                visible: true
//            }


//            PropertyChanges {
//                target: dataText
//                visible: false
//            }

            PropertyChanges {
                target: dataTextInput

                Keys.onPressed: {
                    //console.log("Get key" + event.key);
                    NavigationHandler.HandleTabEvent(event, dataBox);
                    NavigationHandler.HandleArrowEvent(event, dataBox);
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

                cursorPosition: text.length
            }

            PropertyChanges {
                target: dataBox
                z: 1
            }
        }
    ]


}
