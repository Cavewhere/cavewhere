import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

QQ.Rectangle {
    id: delegate

    enum Sort {
        None,
        Ascending,
        Descending
    }

    required property string text;
    required property int columnWidth;

    required property int sortRole; //: -1
    property int sortMode: TableStaticHeaderColumn.Sort.None

    required property var model

    property SortFilterProxyModel _sortModel: model as SortFilterProxyModel

    // Qt6: add cellPadding (and font etc) as public API in headerview
    readonly property real cellPadding: 2

    implicitWidth: textId.implicitWidth + (cellPadding * 2)
    implicitHeight: textId.implicitHeight + (cellPadding * 2)
    color: Theme.surfaceMuted
    border.color: Theme.borderSubtle

    QQ.Connections {
        target: delegate._sortModel
        function onSortRoleChanged() {
            if(delegate._sortModel.sortRole != delegate.sortRole) {
                delegate.sortMode = TableStaticHeaderColumn.Sort.None
            }
        }
    }

    QQ.Item {
        width: delegate.width
        height: delegate.height

        RowLayout {
            anchors.centerIn: parent

            spacing: 1

            QQ.Image {
                id: sortImageId
                visible: delegate.sortMode != TableStaticHeaderColumn.Sort.None
                source: "qrc:/icons/moreArrowDown.png"
                rotation: delegate.sortMode == TableStaticHeaderColumn.Sort.Descending ? 0 : 180
                sourceSize: Qt.size(8, 8)
                smooth: true
            }

            QQ.Item {
                visible: !sortImageId.visible
                implicitWidth: sortImageId.width
            }

            Text {
                id: textId
                text: delegate.text
                color: Theme.text
            }
        }
    }

    QQ.MouseArea {
        anchors.fill: parent
        anchors.leftMargin: 5
        anchors.rightMargin: 5
        onClicked: {
            if(delegate.sortRole >= 0) {
                if(delegate.sortMode == TableStaticHeaderColumn.Sort.None) {
                    delegate.sortMode = TableStaticHeaderColumn.Sort.Ascending
                } else if(delegate.sortMode == TableStaticHeaderColumn.Sort.Ascending) {
                    delegate.sortMode = TableStaticHeaderColumn.Sort.Descending
                } else {
                    //Descending
                    delegate.sortMode = TableStaticHeaderColumn.Sort.Ascending
                }

                //Assume that this is a sort proxy mode
                let sortModel = delegate.model as SortFilterProxyModel
                if(delegate._sortModel) {
                    delegate._sortModel.sortRole = delegate.sortRole
                    switch(delegate.sortMode) {
                    case TableStaticHeaderColumn.Sort.Ascending:
                            delegate._sortModel.sort(Qt.AscendingOrder)
                        break
                    case TableStaticHeaderColumn.Sort.Descending:
                            delegate._sortModel.sort(Qt.DescendingOrder)
                        break
                    }
                } else {
                    console.warn("TableStaticHeaderColumn model isn't a cwSortFilterProxyModel, sorting will not work")
                }
            } else {
                console.warn("Delegate has no sortRole")
            }
        }
    }
}
