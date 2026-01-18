import QtQuick
import QtTest
import cavewherelib

Item {
    id: rootId
    width: 400
    height: 200

    TableStaticColumnModel {
        id: columnModel
        columns: [
            TableStaticColumn {
                id: firstColumn
                columnWidth: 40
                text: "First"
                sortRole: -1
            },
            TableStaticColumn {
                id: secondColumn
                columnWidth: 80
                text: "Second"
                sortRole: -1
            }
        ]
    }

    HorizontalHeaderStaticView {
        id: headerView
        view: tableView
        width: parent.width

        delegate: TableStaticHeaderColumn {
            model: tableView.model
        }
    }

    TableStaticView {
        id: tableView
        y: headerView.height
        width: parent.width
        height: parent.height - headerView.height
        columnModel: columnModel
        model: ListModel {
            ListElement { first: "A"; second: "B" }
        }
        delegate: Item {
            implicitWidth: firstColumn.columnWidth + secondColumn.columnWidth
            implicitHeight: 20
        }
    }

    TestCase {
        name: "TableStaticView"
        when: windowShown

        function init() {
            firstColumn.columnWidth = 40
            secondColumn.columnWidth = 80
        }

        function test_totalWidth() {
            compare(columnModel.totalWidth, 120)
            compare(tableView.implicitWidth, 120)
        }

        function test_columnWidthUpdate() {
            let index = columnModel.index(0, 0)
            columnModel.setData(index, 60, TableStaticColumnModel.ColumnWidthRole)
            compare(firstColumn.columnWidth, 60)
            tryVerify(() => columnModel.totalWidth === 140)
            compare(tableView.implicitWidth, 140)
        }

        function test_headerWidthProvider() {
            compare(headerView.model, columnModel)
            compare(headerView.columnWidthProvider(0), 40)
            compare(headerView.columnWidthProvider(1), 80)
        }
    }
}
