import QtQuick 2.0
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.1
import Cavewhere 1.0

Item {

    function runTestcases() {
        testcases.run();
        helpbox.visible = false
    }

    TestcaseManager {
        id: testcases

        onLineAdded: {
            textArea.append(newLine);
        }

        onResultCleared: {
            textArea.text = ""
        }
    }

    QuoteBox {
        id: helpbox
        pointAtObject: runButton
        pointAtObjectPosition: Qt.point(Math.floor(runButton.width * .5),
                                        runButton.height)

        z: 1
        triangleOffset: 0.8
        color: "#FF9C14"
        borderColor: "black"
        Text {
            text: "Click to run testcases"
        }
    }



    ColumnLayout {
        anchors.fill: parent
        RowLayout {
            Layout.fillWidth: true

            TextField {
                Layout.fillWidth: true
                text: testcases.arguments
                placeholderText: "Arguments. Type <b>--help</b> to get full list"
                enabled: testcases.processState === TestcaseManager.NotRunning;
                onTextChanged: {
                    testcases.arguments = text;
                }
                onAccepted: {
                    runTestcases()
                }
            }

            Button {
                id: runButton
                text: {
                    switch(testcases.processState) {
                    case TestcaseManager.NotRunning:
                        return "Run";
                    case TestcaseManager.Starting:
                        return "Starting";
                    case TestcaseManager.Running:
                        return "Stop"
                    default:
                        return "Error"
                    }
                }

                enabled: {
                    switch(testcases.processState) {
                    case TestcaseManager.NotRunning:
                        return true
                    case TestcaseManager.Starting:
                        return false
                    case TestcaseManager.Running:
                        return true
                    default:
                        return false
                    }
                }

                onClicked: {
                    switch(testcases.processState) {
                    case TestcaseManager.NotRunning:
                        runTestcases()
                        break;
                    case TestcaseManager.Starting:
                        break
                    case TestcaseManager.Running:
                        testcases.stop();
                        break;
                    default:
                        break;
                    }
                }
            }
        }

        TextArea {
            id: textArea
            Layout.fillHeight: true
            Layout.fillWidth: true

            HelpBox {
                anchors.centerIn: parent
                visible: testcases.processState === TestcaseManager.Running &&
                         textArea.text.length === 0


                text: "Testcases are running! Please wait, it will be a while."
            }
        }


    }
}

