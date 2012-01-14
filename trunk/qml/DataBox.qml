import QtQuick 1.1
import Cavewhere 1.0
import "Navigation.js" as NavigationHandler

NavigationRectangle {
    id: dataBox

    property alias dataValue: editor.text
    property alias dataValidator: editor.validator
    property SurveyChunk surveyChunk; //For hooking up signals and slots in subclasses
    property int rowIndex: -1
    property int dataRole

    signal rightClicked(int index)
    signal splitOn(int index)

    //color : Qt.rgba(201, 230, 245, 255);

    //This causes memory leaks in qt 4.7.1!!!
    //    Behavior on y { PropertyAnimation { duration: 250 } }
    //    Behavior on opacity  { PropertyAnimation { duration: 250 } }

    //    onDataValidatorChanged: {
    //        dataTextInput.validator = dataValidator;
    //    }

    function deletePressed() {
        dataValue = '';
        editor.openEditor();
        state = 'EndTyping';
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

    Rectangle {
        id: backgroundStation
        anchors.fill: parent

        gradient: Gradient {
            GradientStop {
                position: rowIndex % 2 === 0 ? 1.0 : 0.0
                color:  "#DDF2FF"
            }
            GradientStop {
                position: rowIndex % 2 === 0 ? 0.4 : 0.6
                color:  "white"
            }
        }

        visible: surveyChunk !== null && surveyChunk.isStationRole(dataRole)

    }

    Rectangle {
        id: backgroundShot
        anchors.fill: parent
        color: "#DDF2FF"

        visible: rowIndex % 2 === 0 && surveyChunk !== null && surveyChunk.isShotRole(dataRole)
    }

    Rectangle {
        id: border
        anchors.fill: parent
        border.color: "lightgray"
        color: "#00000000"
        border.width: 1
    }

    Rectangle {
        id: interalHighlight
        border.color: "black"
        anchors.fill: parent
        anchors.margins: 1
        border.width: 1
        color: "#00000000"
        visible: dataBox.focus || editor.isEditting
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
                deletePressed();
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
        deletePressed()
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
                    dataBox.focus = true;
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
