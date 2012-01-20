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

//    signal rightClicked(int index)
//    signal splitOn(int index)

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
        state = 'MiddleTyping';
    }

    MouseArea {
        anchors.fill: parent

        enabled: !editor.isEditting

        onClicked: {
            dataBox.focus = true
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
            dataBox.state = ""; //Go back to the default state
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
                dataBox.state = 'MiddleTyping'
                editor.openEditor()
                globalShadowTextInput.textInput.text  = event.text
                globalShadowTextInput.clearSelection() //GlobalShowTextInput is what's opened from editor.openEditor
            }
        }
    }

    Keys.onSpacePressed: {
        surveyChunk.parentTrip.addNewChunk();
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
                    if(event.key === Qt.Key_Tab ||
                       event.key === 1 + Qt.Key_Tab //||
//                            event.key === Qt.Key_Left ||
//                            event.key === Qt.Key_Right ||
//                            event.key === Qt.Key_Up ||
//                            event.key === Qt.Key_Down
                            )  {
                            editor.commitChanges()
                    }

                    NavigationHandler.handleTabEvent(event, dataBox);
//                    NavigationHandler.handleArrowEvent(event, dataBox);
                    if(event.accepted) {
                        //Have the editor commit changes
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
                target: editor

                Keys.onSpacePressed: {
                    editor.commitChanges();
                    dataBox.state = '';
                    surveyChunk.parentTrip.addNewChunk();
                }
            }

            PropertyChanges {
                target: globalShadowTextInput

                onEscapePressed: {
                    dataBox.focus = true;
                    dataBox.state = ''; //Default state
                }

                onEnterPressed: {
                    editor.commitChanges();
                    dataBox.nextTabObject.focus = true
                }
            }

            PropertyChanges {
                target: dataBox
                z: 1
            }
        }
    ]
}
