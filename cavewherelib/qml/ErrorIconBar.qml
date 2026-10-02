pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import QtQml.Models as QM
import cavewherelib

// The badge counts roll up the models beneath this one (a node's trips, a
// trip's shots); the popover lists only this model's own messages and counts
// the rest, since those belong to the pages that show them in place.
QQ.Item {
    id: itemId
    objectName: "errorIconBar"

    component ErrorRoleData: QQ.QtObject {
        property int errorType
        property bool suppressed
    }

    property ErrorModel errorModel

    property bool pinned: false

    //A row can outlive the object whose errors it shows by a frame.
    readonly property int fatalCount: itemId.errorModel !== null ? itemId.errorModel.fatalCount : 0
    readonly property int warningCount: itemId.errorModel !== null ? itemId.errorModel.warningCount : 0
    readonly property bool hasErrors: itemId.fatalCount + itemId.warningCount > 0

    readonly property ErrorListModel errors: itemId.errorModel !== null ? itemId.errorModel.errors : null

    // The card's own hover counts, so the pointer can move from the badges onto
    // the card and read it.
    readonly property bool wantsOpen: itemId.hasErrors
                                      && (itemId.pinned
                                          || badgeHoverId.hovered
                                          || cardId.hovered)

    implicitHeight: layoutId.height + Theme.tightSpacing
    implicitWidth: layoutId.width + Theme.tightSpacing

    function togglePinned() {
        if(itemId.pinned) {
            popoverId.close();
        } else if(itemId.hasErrors) {
            itemId.pinned = true;
            openDelayId.stop();
            closeDelayId.stop();
            popoverId.open();
            //The tree view takes the keyboard on the same tap, after this
            //handler, so the card asks for it back once the tap is done.
            Qt.callLater(() => {
                if(itemId.pinned) {
                    popoverId.forceActiveFocus();
                }
            });
        }
    }

    //A pooled tree row is handed another row's model, and the card it had
    //open speaks for the old one.
    onErrorModelChanged: popoverId.close()

    onWantsOpenChanged: {
        if(itemId.wantsOpen) {
            closeDelayId.stop();
            if(!popoverId.opened) {
                openDelayId.restart();
            }
        } else {
            openDelayId.stop();
            closeDelayId.restart();
        }
    }

    QQ.HoverHandler {
        id: badgeHoverId
    }

    QQ.TapHandler {
        onTapped: itemId.togglePinned()
    }

    // A pointer sweeping down the column passes over many rows' badges, so a
    // peek waits the way a tooltip does.
    QQ.Timer {
        id: openDelayId
        interval: Theme.toolTipDelay

        onTriggered: popoverId.open()
    }

    QQ.Timer {
        id: closeDelayId
        interval: Theme.errorPopoverHoverCloseDelay

        onTriggered: popoverId.close()
    }

    RowLayout {
        id: layoutId

        anchors.centerIn: parent

        spacing: Theme.tightSpacing
        QQ.Image {
            objectName: "fatalBadge"
            source: "qrc:icons/svg/stopSignError.svg"
            sourceSize: Qt.size(Theme.iconSizeButton, Theme.iconSizeButton)
            visible: itemId.fatalCount > 0
        }

        QQ.Image {
            objectName: "warningBadge"
            source: "qrc:icons/svg/warning.svg"
            sourceSize: Qt.size(Theme.iconSizeButton, Theme.iconSizeButton)
            visible: itemId.warningCount > 0
        }

        QQ.Image {
            objectName: "noErrorBadge"
            source: "qrc:icons/good.png"
            sourceSize: Qt.size(Theme.iconSizeButton, Theme.iconSizeButton)
            visible: !itemId.hasErrors
        }
    }

    QC.Popup {
        id: popoverId
        objectName: "errorPopover"

        y: itemId.height
        width: Theme.errorPopoverWidth
        padding: 0
        background: null
        // Outside the parent, so a tap on the badges reaches the badges and
        // unpins rather than closing and pinning again on the same press.
        closePolicy: QC.Popup.CloseOnEscape | QC.Popup.CloseOnPressOutsideParent

        onClosed: itemId.pinned = false

        contentItem: FlyoutCard {
            id: cardId

            title: {
                const counts = [];
                if(itemId.fatalCount > 0) {
                    counts.push(qsTr("%n error(s)", "", itemId.fatalCount));
                }
                if(itemId.warningCount > 0) {
                    counts.push(qsTr("%n warning(s)", "", itemId.warningCount));
                }
                return counts.join(", ");
            }
            iconSource: itemId.fatalCount > 0
                        ? "qrc:icons/svg/stopSignError.svg"
                        : "qrc:icons/svg/warning.svg"

            onCloseRequested: popoverId.close()

            // Built while the card is shown and not before, so a tree full of
            // rows carries no filtered copies of lists nobody is reading.
            QQ.Loader {
                Layout.fillWidth: true
                Layout.margins: Theme.toolFlyoutPadding
                active: popoverId.visible
                sourceComponent: messageListComponentId
            }
        }
    }

    QQ.Component {
        id: messageListComponentId

        ColumnLayout {
            id: messageListId

            readonly property int descendantCount: itemId.fatalCount + itemId.warningCount
                                                   - fatalRepeaterId.count - warningRepeaterId.count

            spacing: Theme.sectionSpacing

            // A fatal error counts whether or not it is suppressed and a
            // warning only while it is not, the way the model counts them, so
            // the list and the badges agree.
            QM.SortFilterProxyModel {
                id: fatalErrorsId
                model: itemId.errors
                filters: [
                    QM.FunctionFilter {
                        function filter(data: ErrorRoleData): bool {
                            return data.errorType === CwError.Fatal
                        }
                    }
                ]
            }

            QM.SortFilterProxyModel {
                id: warningErrorsId
                model: itemId.errors
                filters: [
                    QM.FunctionFilter {
                        function filter(data: ErrorRoleData): bool {
                            return data.errorType === CwError.Warning && !data.suppressed
                        }
                    }
                ]
            }

            QQ.Repeater {
                id: fatalRepeaterId
                model: fatalErrorsId
                delegate: messageComponentId
            }

            QQ.Repeater {
                id: warningRepeaterId
                model: warningErrorsId
                delegate: messageComponentId
            }

            QC.Label {
                objectName: "descendantErrorsLabel"
                Layout.fillWidth: true
                visible: messageListId.descendantCount > 0
                text: qsTr("%n more in the survey data it holds", "", messageListId.descendantCount)
                color: Theme.textSubtle
                wrapMode: QC.Label.WordWrap
            }
        }
    }

    QQ.Component {
        id: messageComponentId

        RowLayout {
            id: messageRowId

            required property int errorType
            required property string message
            required property string detail

            Layout.fillWidth: true
            spacing: Theme.flowSpacing

            QQ.Image {
                Layout.alignment: Qt.AlignTop
                source: messageRowId.errorType === CwError.Fatal
                        ? "qrc:icons/svg/stopSignError.svg"
                        : "qrc:icons/svg/warning.svg"
                sourceSize: Qt.size(Theme.iconSizeButton, Theme.iconSizeButton)
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.tightSpacing

                QC.Label {
                    objectName: "errorMessage"
                    Layout.fillWidth: true
                    text: messageRowId.message
                    color: Theme.text
                    wrapMode: QC.Label.WordWrap
                }

                QC.Label {
                    Layout.fillWidth: true
                    visible: messageRowId.detail !== ""
                    text: messageRowId.detail
                    color: Theme.textSubtle
                    font.pixelSize: Theme.fontSizeCaption
                    wrapMode: QC.Label.WordWrap
                }
            }
        }
    }
}
