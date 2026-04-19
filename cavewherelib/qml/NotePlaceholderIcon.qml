import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

QQ.Rectangle {
    id: rootId

    property QQ.QtObject noteObject: null

    readonly property bool isSketch: noteObject !== null && noteObject instanceof Sketch
    readonly property string fileName: {
        if (noteObject === null) {
            return ""
        }
        if (isSketch) {
            switch (noteObject.viewType) {
            case Sketch.Plan: return qsTr("Plan")
            case Sketch.RunningProfile: return qsTr("Running Profile")
            case Sketch.ProjectedProfile: return qsTr("Projected Profile")
            }
            return qsTr("Sketch")
        }
        if (noteObject.filename !== undefined && noteObject.filename !== "") {
            return RootData.fileName(noteObject.filename)
        }
        if (noteObject.name !== undefined && noteObject.name !== "") {
            return noteObject.name
        }
        return ""
    }
    readonly property string fileExtension: {
        const dotIndex = fileName.lastIndexOf(".")
        if (dotIndex > 0 && dotIndex < fileName.length - 1) {
            return fileName.slice(dotIndex + 1).toUpperCase()
        }
        return qsTr("GLB")
    }
    readonly property real _shortSide: Math.min(width, height)
    readonly property real _scaleFactor: _shortSide / 200

    color: Theme.surfaceRaised
    border.color: Theme.border
    border.width: _shortSide < 100 ? 1 : 2
    radius: 6

    QQ.Column {
        anchors.centerIn: parent
        spacing: Math.max(2, Math.round(6 * rootId._scaleFactor))
        width: parent.width - Math.max(8, Math.round(16 * rootId._scaleFactor))

        Icon {
            visible: rootId.isSketch
            source: "qrc:/twbs-icons/icons/pencil-square.svg"
            width: Math.max(20, Math.round(48 * rootId._scaleFactor))
            height: width
            sourceSize: Qt.size(width, height)
            anchors.horizontalCenter: parent.horizontalCenter
            smooth: true
            colorizationColor: Theme.text
            colorizeEnabled: rootId.isSketch
        }

        QC.Label {
            visible: !rootId.isSketch
            text: rootId.fileExtension
            font.bold: true
            font.pixelSize: Math.max(12, Math.round(32 * rootId._scaleFactor))
            color: Theme.text
            horizontalAlignment: QQ.Text.AlignHCenter
            width: parent.width
        }

        QC.Label {
            visible: text.length > 0
            text: rootId.fileName
            font.bold: true
            font.pixelSize: Math.max(Theme.fontSizeCaption, Math.round(Theme.fontSizeUI * rootId._scaleFactor))
            color: Theme.textSecondary
            wrapMode: QQ.Text.WordWrap
            horizontalAlignment: QQ.Text.AlignHCenter
            width: parent.width
            elide: QC.Label.ElideRight
            maximumLineCount: 2
        }
    }
}
