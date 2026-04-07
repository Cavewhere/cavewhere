import QtQuick
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

ColumnLayout {
    id: root

    required property GitHubIntegration gitHubIntegration

    property string errorMessage
    property alias repoName: repoNameFieldId.text

    signal createRequested(string repoName, bool isPrivate)

    spacing: 8

    function _isDuplicateName(name) {
        let model = root.gitHubIntegration.repositories
        if (!model) { return false }
        for (let i = 0; i < model.rowCount(); i++) {
            let idx = model.index(i, 0)
            let existingName = String(model.data(idx, Qt.DisplayRole))
            if (existingName.toLowerCase() === name.toLowerCase()) {
                return true
            }
        }
        return false
    }

    QC.Label {
        Layout.fillWidth: true
        text: qsTr("Repository name")
        color: Theme.textSecondary
    }

    QC.TextField {
        id: repoNameFieldId
        objectName: "repoNameField"
        Layout.fillWidth: true
        placeholderText: qsTr("my-cave-map")
        onTextChanged: root.errorMessage = ""
    }

    QC.Label {
        text: qsTr("Visibility")
        color: Theme.textSecondary
    }

    QC.ButtonGroup { id: visibilityGroupId }

    RowLayout {
        Layout.fillWidth: true
        spacing: 0

        QC.Button {
            id: privateButtonId
            text: qsTr("Private")
            checkable: true
            checked: true
            flat: true
            highlighted: checked
            QC.ButtonGroup.group: visibilityGroupId
        }

        QC.Button {
            id: publicButtonId
            text: qsTr("Public")
            checkable: true
            flat: true
            highlighted: checked
            QC.ButtonGroup.group: visibilityGroupId
        }

        Item { Layout.fillWidth: true }
    }

    QC.Label {
        Layout.fillWidth: true
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        color: Theme.textSubtle
        text: privateButtonId.checked
            ? qsTr("Only you and collaborators can see this repository. Recommended for cave survey data.")
            : qsTr("Anyone on the internet can view this repository and its survey data.")
    }

    QC.Label {
        Layout.fillWidth: true
        visible: root.errorMessage.length > 0
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        color: Theme.danger
        text: root.errorMessage
    }

    QC.Button {
        id: createButtonId
        objectName: "createButton"
        text: qsTr("Create repository")
        enabled: repoNameFieldId.text.trim().length > 0
        onClicked: {
            let name = repoNameFieldId.text.trim()
            if (root._isDuplicateName(name)) {
                root.errorMessage = qsTr("A repository named '%1' already exists in your account. "
                    + "Choose a different name or use 'Connect existing' to wire it as your remote.")
                    .arg(name)
                return
            }
            root.createRequested(name, privateButtonId.checked)
        }
    }
}
