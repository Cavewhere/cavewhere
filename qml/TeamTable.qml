/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// import QtQuick 2.0 as QQ // to target S60 5th Edition or Maemo 5
import QtQuick 2.0 as QQ
import Cavewhere 1.0

QQ.Item {
    id: teamTable

    property Team model

    height: childrenRect.height

    function updateState() {
        if(teamList.count > 0) {
            teamTable.state = ""
        } else {
            teamTable.state = "NoTeam"
        }
    //        console.debug("Update state: " + teamTable.state )
    }

    SectionLabel {
        id: title
        text: "Team"
    }

    QQ.Item {
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
                teamList.currentIndex = teamList.count - 1
            }
            visible: true
        }

        Text {
            id: nameHeader
            anchors.left: parent.left
            anchors.right: parent.horizontalCenter

            horizontalAlignment: Text.AlignHCenter
            text: "Name"
            font.bold: true
            visible: teamList.visible
        }

        Text {
            id: jobHeader
            anchors.left: parent.horizontalCenter
            anchors.right: parent.right

            horizontalAlignment: Text.AlignHCenter
            text: "Role"
            font.bold: true
            visible: teamList.visible
        }

        QQ.Rectangle {
            id: verticalLine2
            anchors.left: parent.horizontalCenter
            anchors.top: parent.top
            anchors.bottom: teamList.top
            width: 1
            border.color: "lightgray"
            border.width: 1
            visible: teamList.visible
        }

        QQ.ListView {
            id: teamList

            anchors.top: addPerson.bottom

            width: teamTable.width
            height: childrenRect.height

            model: teamTable.model
            interactive: false

            visible: true

            //            onVisibleChanged: {
            //                console.log("Visible changed:" + visible + " state: " + teamTable.state)
            //            }

            onCountChanged: {
                updateState();
            }

            delegate: QQ.Rectangle {
                id: rowDelegate

                width: teamList.width
                height: Math.max(25, Math.max(personNameRow.height, jobsListView.height)) + 6

                color: index % 2 === 0 ? "#DDF2FF" : "white"

                property bool selected: teamList.currentIndex === index

                QQ.MouseArea {
                    id: rowMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        teamList.currentIndex = index
                        personNameRow.forceActiveFocus()
                    }
                    onDoubleClicked: nameText.openEditor()
                }

                QQ.Rectangle {
                    height: 1
                    border.width: 1
                    border.color: "lightgray"
                    anchors.left: parent.left
                    anchors.right: parent.right
                }

                QQ.Rectangle {
                    id: selectedBackground
                    border.width: 1
                    anchors.fill: parent
                    anchors.margins: 1
                    color: "#00000000"
                    z:1
                    visible: rowDelegate.selected
                }

                QQ.Item {
                    id: personNameRow

                    anchors.left: parent.left
                    anchors.leftMargin: 4
                    anchors.right: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter

                    RemoveButton {
                        id: deletePersonButton
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter

                        onClicked: {
                            teamTable.model.removeTeamMember(index)
                        }


                        visible: rowDelegate.selected
                    }

                    DoubleClickTextInput {
                        id: nameText
                        text: name

                        anchors.left: deletePersonButton.right
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter

                        onFinishedEditting: {
                            //Set the team name data
                            teamTable.model.setData(index, Team.NameRole, newText)
                        }
                    }
                }

                QQ.Rectangle {
                    id: verticalLine
                    anchors.left: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 1
                    border.color: "lightgray"
                    border.width: 1
                }

                QQ.Flow {
                    id: jobsListView

                    property int rowIndex: index

                    anchors.left: parent.horizontalCenter
                    anchors.leftMargin: 3
                    anchors.right: parent.right
                    anchors.verticalCenter:  parent.verticalCenter

                    spacing: 5


                    QQ.Repeater {
                        model: jobs
                        delegate: QQ.Rectangle {
                            id: job
                            property alias selected: job.focus

                            radius: 5
                            color: selected ? "#F39935" : "#E4CC99"
                            border.width: 1
                            border.color: selected ? "#FFEDC7" : "#FFF3D6"

                            width: Math.max(jobText.width, 20) + 10
                            height: jobText.height + 6

                            function removeJob() {
                                var alljobs = jobs
                                alljobs.splice(index, 1)
                                teamTable.model.setData(jobsListView.rowIndex, Team.JobsRole, alljobs)
                            }

                            DoubleClickTextInput {
                                id: jobText
                                anchors.centerIn: parent
                                text: modelData

                                onFinishedEditting: {
                                    var alljobs = jobs

                                    if(newText === "") {
                                        //Remove the job if there's no data
                                        alljobs.splice(index, 1)
                                    } else {
                                        //Set the new text for the job
                                        alljobs[index] = newText
                                    }
                                    teamTable.model.setData(jobsListView.rowIndex, Team.JobsRole, alljobs)
                                }
                            }

                            QQ.MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    teamList.currentIndex = jobsListView.rowIndex
                                    job.forceActiveFocus()
                                }

                                onDoubleClicked: {
                                    jobText.openEditor()
                                }
                            }

                            QQ.Keys.onDeletePressed: {
                                removeJob()
                            }
                        }
                    }

                    QQ.Rectangle {
                        radius: 5
                        color: "#B2D3AF"

                        border.width: 1
                        border.color: "#E6FFE4"

                        width: addJobRow.width + 6
                        height: addJobRow.height + 6

                        visible: rowDelegate.selected

                        QQ.Row {
                            id: addJobRow
                            anchors.centerIn: parent
                            spacing: 5

                            QQ.Image {
                                source: "qrc:icons/plus.png"
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Text {
                                id: addText
                                anchors.verticalCenter: parent.verticalCenter
                                text: "Role"
                            }
                        }

                        QQ.MouseArea {
                            anchors.fill: parent

                            onClicked: {
                                var alljobs = jobs
                                alljobs.push("Role " + (jobs.length + 1));
                                teamTable.model.setData(index, Team.JobsRole, alljobs)
                            }
                        }
                    }
                }
            }
        }
    }

    onModelChanged: {
        //        console.debug("Model changed:" + model + " " + model.count)
        teamList.currentIndex = -1
        updateState()
    }


    states: [
        QQ.State {
            name: "NoTeam"
            //            when: teamList.count === 0
            QQ.AnchorChanges {
                target: addPerson
                anchors.left: undefined
                anchors.horizontalCenter: personTable.horizontalCenter
            }

            QQ.PropertyChanges {
                target: addPerson
                anchors.leftMargin: 0
                text: "Add a team member"
            }

            QQ.PropertyChanges {
                target: teamList
                height: 0
            }

            QQ.PropertyChanges {
                target: nameHeader
                visible: false
            }

            QQ.PropertyChanges {
                target: jobHeader
                visible: false
            }

            QQ.PropertyChanges {
                target: verticalLine2
                visible: false
            }
        }
    ]
}
