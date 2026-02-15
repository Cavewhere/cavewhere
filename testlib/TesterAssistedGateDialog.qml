import QtQuick
import QtQuick.Controls as QC
import cw.TestLib

Item {
    id: gateId

    visible: false
    width: 0
    height: 0

    // Host item where the modal should be centered.
    required property Item dialogParent

    // Human-readable context shown to testers.
    property string testName: ""
    property string requirements: ""

    // Global behavior:
    // - prompt (default): show dialog
    // - skip: skip all tester-assisted tests
    // - run_all: run all tester-assisted tests without prompting
    property string modeEnvironmentVariable: "CW_QML_TESTER_ASSISTED_MODE"
    property int autoSkipCountdownSeconds: 5

    // Decision for the current test invocation.
    property bool decisionReady: false
    property bool runCurrentTest: false

    // Session-wide mode remembered after the first decision.
    property bool runAllAssistedTests: false
    property bool skipAllAssistedTests: false

    readonly property int decisionTimeoutMs: Math.max(10000, (autoSkipCountdownSeconds + 2) * 1000)

    function beginDecision(testName, requirements) {
        gateId.testName = testName
        gateId.requirements = requirements
        gateId.decisionReady = false
        gateId.runCurrentTest = false

        if (gateId.runAllAssistedTests) {
            gateId.runCurrentTest = true
            gateId.decisionReady = true
            return
        }

        if (gateId.skipAllAssistedTests) {
            gateId.decisionReady = true
            return
        }

        const mode = TestHelper.environmentVariable(gateId.modeEnvironmentVariable).trim().toLowerCase()
        if (mode === "skip") {
            chooseSkipAll()
            return
        }
        if (mode === "run_all") {
            chooseRunAll()
            return
        }

        remainingSeconds = Math.max(1, gateId.autoSkipCountdownSeconds)
        decisionDialog.open()
    }

    function chooseSkipAll() {
        gateId.skipAllAssistedTests = true
        gateId.runCurrentTest = false
        gateId.decisionReady = true
        decisionDialog.close()
    }

    function chooseRunThis() {
        gateId.runCurrentTest = true
        gateId.decisionReady = true
        decisionDialog.close()
    }

    function chooseRunAll() {
        gateId.runAllAssistedTests = true
        gateId.runCurrentTest = true
        gateId.decisionReady = true
        decisionDialog.close()
    }

    property int remainingSeconds: autoSkipCountdownSeconds

    Timer {
        id: countdownTimer
        interval: 1000
        running: decisionDialog.visible
        repeat: true
        onTriggered: {
            remainingSeconds -= 1
            if (remainingSeconds <= 0) {
                gateId.chooseSkipAll()
            }
        }
    }

    QC.Dialog {
        id: decisionDialog
        parent: gateId.dialogParent
        anchors.centerIn: parent
        modal: true
        closePolicy: QC.Popup.NoAutoClose
        title: "Tester-Assisted Test"
        width: 560

        contentItem: Column {
            spacing: 10

            QC.Label {
                width: decisionDialog.availableWidth
                wrapMode: Text.WordWrap
                text: "This is a user-driven validation test and requires a tester in the loop."
            }

            QC.Label {
                width: decisionDialog.availableWidth
                wrapMode: Text.WordWrap
                font.bold: true
                visible: gateId.testName.length > 0
                text: "Test: " + gateId.testName
            }

            QC.Label {
                width: decisionDialog.availableWidth
                wrapMode: Text.WordWrap
                visible: gateId.requirements.length > 0
                text: "Requirements:\n" + gateId.requirements
            }

            QC.Label {
                width: decisionDialog.availableWidth
                wrapMode: Text.WordWrap
                text: "Auto-skip in " + remainingSeconds + "s."
            }
        }

        footer: QC.DialogButtonBox {
            QC.Button {
                text: "Skip Assisted Tests"
                QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.RejectRole
                onClicked: gateId.chooseSkipAll()
            }

            QC.Button {
                text: "Run This Test"
                QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.AcceptRole
                onClicked: gateId.chooseRunThis()
            }

            QC.Button {
                text: "Run All Assisted Tests"
                QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.AcceptRole
                onClicked: gateId.chooseRunAll()
            }
        }
    }
}
