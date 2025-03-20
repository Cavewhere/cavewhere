import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cavewherelib

Rectangle {
    id: delegate

    enum Sort {
        None,
        Ascending,
        Descending
    }

    required property string text;
    required property int columnWidth;

    property int sortRole: -1
    property int sortMode: TableStaticColumn.Sort.None

    required property var model

    property SortFilterProxyModel _sortModel: model as SortFilterProxyModel

    // Qt6: add cellPadding (and font etc) as public API in headerview
    readonly property real cellPadding: 2

    implicitWidth: textId.implicitWidth + (cellPadding * 2)
    implicitHeight: textId.implicitHeight + (cellPadding * 2)
    color: "#f6f6f6"
    border.color: "#e4e4e4"

    Connections {
        target: delegate._sortModel
        function onSortRoleChanged() {
            if(delegate._sortModel.sortRole != delegate.sortRole) {
                delegate.sortMode = TableStaticColumn.Sort.None
            }
        }
    }

    Item {
        width: delegate.width
        height: delegate.height

        RowLayout {
            anchors.centerIn: parent

            spacing: 1

            Image {
                id: sortImageId
                visible: delegate.sortMode != TableStaticColumn.Sort.None
                source: "qrc:/icons/moreArrowDown.png"
                rotation: delegate.sortMode == TableStaticColumn.Sort.Descending ? 0 : 180
                sourceSize: Qt.size(8, 8)
                smooth: true
            }

            Item {
                visible: !sortImageId.visible
                implicitWidth: sortImageId.width
            }

            Label {
                id: textId
                text: delegate.text
                color: "#ff26282a"
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        anchors.leftMargin: 5
        anchors.rightMargin: 5
        onClicked: {
            if(delegate.sortRole >= 0) {
                if(delegate.sortMode == TableStaticColumn.Sort.None) {
                    delegate.sortMode = TableStaticColumn.Sort.Ascending
                } else if(delegate.sortMode == TableStaticColumn.Sort.Ascending) {
                    delegate.sortMode = TableStaticColumn.Sort.Descending
                } else {
                    //Descending
                    delegate.sortMode = TableStaticColumn.Sort.Ascending
                }

                //Assume that this is a sort proxy mode
                let sortModel = delegate.model as SortFilterProxyModel
                if(delegate._sortModel) {
                    delegate._sortModel.sortRole = delegate.sortRole
                    switch(delegate.sortMode) {
                    case TableStaticColumn.Sort.Ascending:
                            delegate._sortModel.sort(Qt.AscendingOrder)
                        break
                    case TableStaticColumn.Sort.Descending:
                            delegate._sortModel.sort(Qt.DescendingOrder)
                        break
                    }
                } else {
                    console.warn("TableStaticColumn model isn't a cwSortFilterProxyModel, sorting will not work")
                }
            }
        }
    }
}
