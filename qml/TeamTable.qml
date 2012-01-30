// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 1.1
import Cavewhere 1.0

Item {
    id: teamTable

    property Team model

    height: childrenRect.height


    SectionLabel {
        id: title
        text: "Team"
    }

    Rectangle {
        id: personTable

        anchors.top: title.bottom

        width: teamList.width
        height: childrenRect.height

        AddButton {
            id: addPerson
            anchors.left:  parent.left
            //            anchors.leftMargin: 10

            onClicked: {
                model.addTeamMember();
            }
        }

        Text {
            id: nameHeader
            anchors.left: parent.left
            anchors.right: parent.horizontalCenter

            horizontalAlignment: Text.AlignHCenter
            text: "Name"
            font.bold: true
        }

        Text {
            id: jobHeader
            anchors.left: parent.horizontalCenter
            anchors.right: parent.right

            horizontalAlignment: Text.AlignHCenter
            text: "Job"
            font.bold: true
        }

        ListView {
            id: teamList

            anchors.top: addPerson.bottom

            width: teamTable.width
            height: childrenRect.height

            model: teamTable.model
            interactive: false

            delegate: Rectangle {
                id: rowDelegate

                width: teamList.width
                height: childrenRect.height

                color: index % 2 === 0 ? "#DDF2FF" : "white"

                Rectangle {
                    height: 1
                    border.width: 1
                    border.color: "lightgray"
                    anchors.left: parent.left
                    anchors.right: parent.right
                }

                Rectangle {
                    border.width: 1
                    x: rowDelegate.x + 1
                    y: personNameRow.y + 1
                    width: Math.floor(teamTable.width / 2.0) - 2
                    height: personNameRow.height - 2
                    color: "#00000000"
                }

                Row {
                    id: personNameRow

                    anchors.left: parent.left
                    anchors.leftMargin: 2

                    height: nameText.height + 6

                    RemoveButton {
                        id: deletePersonButton
                        anchors.verticalCenter: parent.verticalCenter
                        height: 15

                        onClicked: {
                            teamTable.model.removeTeamMember(index)
                        }
                    }

                    DoubleClickTextInput {
                        id: nameText
                        text: name

                        anchors.verticalCenter: parent.verticalCenter

                        onFinishedEditting: {
                            teamTable.model.setData(index, Team.NameRole, newText)
                        }
                    }
                }

                ListView {
                    id: jobsListView
                    model: jobs

                    property int delegateHeight: 20

                    x: Math.floor(teamTable.width / 2.0) + 5

                    height: delegateHeight * count
                    width: teamTable.width / 2.0

                    interactive: false

                    delegate: Text {
                        height: jobsListView.delegateHeight
                        text: modelData
                    }
                }
            }
        }


    }

    Rectangle {
        id: verticalLine
        anchors.left: personTable.horizontalCenter
        anchors.top: personTable.top
        anchors.bottom: personTable.bottom
        width: 1
        border.color: "lightgray"
        border.width: 1
    }


    states: [
        State {
            name: "HasTeam"
            when: teamList.count === 0

            AnchorChanges {
                target: addPerson
                anchors.left: undefined
                anchors.horizontalCenter: personTable.horizontalCenter
            }

            PropertyChanges {
                target: addPerson
                anchors.leftMargin: 0
                text: "Add a team member"
            }

            PropertyChanges {
                target: verticalLine
                visible: false
            }

            PropertyChanges {
                target: teamList
                visible: false
            }

            PropertyChanges {
                target: nameHeader
                visible: false
            }

            PropertyChanges {
                target: jobHeader
                visible: false
            }
        }
    ]
}
