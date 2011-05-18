import Qt 4.7
//import "Cavewhere" 1.0
import "SurveyChunk.js" as SurveyEditor

Rectangle {
    id: page

    property variant surveyData

    height: childrenRect.height

    Rectangle {
        id: removeButton
        color: "red"

        width: 20
        height: 20

        anchors.bottom: stationTitleLabel.top;
        anchors.left: stationTitleLabel.left;

        MouseArea {
            anchors.fill: parent
            onClicked: {
                console.log("Remove");
                console.log("Page:" + page.width + page.height);
            }
        }
    }

    Rectangle {
        id: addButton
        color: "green"

        width: 20
        height: 20

        anchors.left: removeButton.right;
        anchors.top: removeButton.top;

        MouseArea {
            anchors.fill: parent
            onClicked: {
                console.log("Added");
                surveyData.AddShot();
            }
        }
    }

    TitleLabel {
        id: stationTitleLabel
        text: "Station"
        x: 0;
        y: 0;
    }

    TitleLabel {
        id: distanceTitleLabel
        text: "Distance"
        anchors.left: stationTitleLabel.right
        anchors.top: stationTitleLabel.top
        anchors.topMargin: height / 2
    }

    TitleLabel {
        id: azimuthTitleLabel
        text: "Azimuth"
        anchors.left: distanceTitleLabel.right
        anchors.top: distanceTitleLabel.top
    }

    TitleLabel {
        id: clinoTitleLabel
        text: "Vertical\n Angle"
        anchors.left: azimuthTitleLabel.right
        anchors.top: azimuthTitleLabel.top

    }

    TitleLabel {
        id: leftTitleLabel
        text: "L"
        anchors.left: clinoTitleLabel.right
        anchors.top: stationTitleLabel.top
    }

    TitleLabel {
        id: rightTitleLabel
        text: "R"
        anchors.left: leftTitleLabel.right
        anchors.top: stationTitleLabel.top
    }

    TitleLabel {
        id: upTitleLabel
        text: "U"
        anchors.left: rightTitleLabel.right
        anchors.top: stationTitleLabel.top
    }

    TitleLabel {
        id: downTitleLabel
        text: "D"
        anchors.left: upTitleLabel.right
        anchors.top: stationTitleLabel.top

    }


    Component.onCompleted: {
        SurveyEditor.Parent = page;

        SurveyEditor.StationTitelLabel = stationTitleLabel;
        SurveyEditor.LeftTitleLabel = leftTitleLabel;
        SurveyEditor.RightTitleLabel = rightTitleLabel;
        SurveyEditor.UpTitleLabel = upTitleLabel;
        SurveyEditor.DownTitleLabel = downTitleLabel;
        SurveyEditor.DistanceTitleLabel = distanceTitleLabel;
        SurveyEditor.ClinoTitleLabel = clinoTitleLabel;
        SurveyEditor.CompassTitleLabel = azimuthTitleLabel;

        reload();
    }

    onSurveyDataChanged: reload()


    /**
    \brief Reloads the chunk
    */
    function reload() {
        if(surveyData == null) { return; }

        SurveyEditor.SurveyModel = surveyData;
        SurveyEditor.CreateView();
        surveyData.StationAdded.connect(SurveyEditor.CreateView);
    }



}

