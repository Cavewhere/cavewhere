/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

DataBox {
    id: dataBoxId

    property bool distanceIncluded: true //This show's if the distance is include (true) or excluded

    ShotMenu {
        id: removeMenuId
        model: dataBoxId.model
        dataValue: dataBoxId.dataValue
        listViewIndex: dataBoxId.listViewIndex
        removePreview: dataBoxId.removePreview
    }

    rightClickMenuLoader: removeMenuId

    DataBoxBadge {
        visible: !dataBoxId.distanceIncluded
        text: "Excluded"
    }

    DataBoxCaret {
        active: dataBoxId.focus
        buttonObjectName: "excludeMenuButton"

        menu: QC.Menu {
            objectName: "excludeMenuId"

            QC.MenuItem {
                objectName: "excludeDistanceMenuItem"
                text: dataBoxId.distanceIncluded ? "Exclude Distance" : "Include Distance"
                onTriggered: {
                    dataBoxId.model.setDataAt(dataBoxId.model.cellIndex(dataBoxId.listViewIndex,
                                                                        SurveyEditorCellIndex.ShotDistanceIncludedCell),
                                              !dataBoxId.distanceIncluded)
                }
            }
        }
    }
}
