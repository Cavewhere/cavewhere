import QtQuick
import QtQuick.Controls as QC
import cavewherelib
QC.HorizontalHeaderView {
    id: horizontalHeader
    // Layout.fillWidth: true

    required property TableStaticView view

    model: view.columnModel
    textRole: "text"

    columnWidthProvider: function (column) {
        let columnWidth = explicitColumnWidth(column);
        if(columnWidth === -1) {
            //column width hasn't been set.
            let index = view.columnModel.index(column, 0);
            return view.columnModel.data(index, TableStaticColumnModel.ColumnWidthRole);
        }

        return Math.min(300, Math.max(50, columnWidth));
    }

    clip: true

    onLayoutChanged: {
        // Update each ListElement's columnWidth with the current header
        for(let i = 0; i < view.columnModel.count; i++) {
            let columnWidth = horizontalHeader.columnWidth(i);
            if(columnWidth !== -1) {
                let index = view.columnModel.index(i, 0);
                let currentWidth = view.columnModel.data(index, TableStaticColumnModel.ColumnWidthRole);
                if(currentWidth !== columnWidth) {
                    view.columnModel.setData(index, columnWidth, TableStaticColumnModel.ColumnWidthRole);
                }
            }
        }
    }
}
