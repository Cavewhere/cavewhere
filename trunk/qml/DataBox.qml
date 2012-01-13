import QtQuick 1.1
import Cavewhere 1.0
import "Navigation.js" as NavigationHandler

NavigationRectangle {
    id: dataBox

    property alias dataValue: editor.text
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

    MouseArea {
        anchors.fill: parent

        onClicked: {
            dataBox.focus = true
        }

        onDoubleClicked: {
            editor.focus = true;
        }
    }

    DoubleClickTextInput {
        id: editor
        anchors.fill: parent

        onFinishedEditting: {
            surveyChunk.setData(dataRole, rowIndex, newText)
        }

        onStartedEditting: {
            dataBox.state = 'MiddleTyping';
        }
    }

    Keys.onPressed: {
        NavigationHandler.handleTabEvent(event, dataBox);
        NavigationHandler.handleArrowEvent(event, dataBox);

        if(!event.accepted) {
            if(event.key === Qt.Key_Backspace) {
                state = 'EndTyping';
                editor.openEditor()
                dataValue = dataValue.substring(0, dataValue.length - 1);
                return;
            }

            if(editor.validator.validate(event.text) > 0 && event.text.length > 0) {
                dataBox.state = 'EndTyping'
                editor.openEditor()
                globalShadowTextInput.textInput.text  = event.text
                globalShadowTextInput.clearSelection() //GlobalShowTextInput is what's opened from editor.openEditor
            }
        }
    }

    Keys.onSpacePressed: {
        dataBox.splitOn(rowIndex); //Emit signal
    }

    Keys.onEnterPressed: {
        editor.openEditor()
    }

    Keys.onReturnPressed: {
        editor.openEditor()
    }

    Keys.onDeletePressed: {
        dataValue = '';
        editor.openEditor();
        state = 'EndTyping';
    }

    states: [

        State {
            name: "MiddleTyping"

            PropertyChanges {
                target: globalShadowTextInput.editor
                Keys.onPressed: {
                    NavigationHandler.handleTabEvent(event, dataBox);
                    if(event.accepted) {
                        //Have the editor commit changes
                        editor.commitChanges()
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
                target: globalShadowTextInput

                onEscapePressed: {
                    dataBox.state = ''; //Default state
                }

                onEnterPressed: {
                    dataBox.nextTabObject.focus = true
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
                target: globalShadowTextInput.editor

                Keys.onPressed: {

                    NavigationHandler.handleTabEvent(event, dataBox);
                    NavigationHandler.handleArrowEvent(event, dataBox);
                    NavigationHandler.enterNavigation(event, dataBox);
                    if(event.accepted) {
                        editor.commitChanges()
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
                target: globalShadowTextInput.textInput
                cursorPosition: text.length
            }

            PropertyChanges {
                target: dataBox
                z: 1
            }
        }
    ]
}
