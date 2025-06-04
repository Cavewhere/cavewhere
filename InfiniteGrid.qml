import QtQuick
import CaveWhereSketch
import cavewherelib

Item {
    id: gridId

    property double msaaScale: 2.0 //Higher value will cause rendering aliasing
    property double viewScale: 1.0
    property alias viewport: gridModel.viewport
    property alias worldToScreenMatrix: gridModel.mapMatrix //Holds the map scaling
    property alias gridOrigin: gridModel.gridOrigin

    function msaaGridVisible(lineWidth) {
        let spacing = 1.0 / Math.sqrt(RootDataSketch.sampleCount * msaaScale);
        let scaledLineWidth = viewScale * lineWidth;
        return scaledLineWidth > spacing;
    }

    InfiniteGridModel {
        id: gridModel
        viewport: containerId.viewport
        viewScale: 1.0 / gridId.viewScale
        lineColor: "#1eb6dd"
        labelColor: "#178ba8"
    }

    ShapePathInstantiator {
        anchors.fill: parent
        model: gridModel.minorGridModel
    }

    ShapePathInstantiator {
        anchors.fill: parent
        model: gridModel.majorGridModel
    }




    //ListView likely gives better preformance than a repeater because
    //they pool and re-use items, although this a abusive use of a list view
    component LabelView : ListView {
        id: repeaterId

        //Need to have some size or the text labels are generated
        width: 1
        height: 1

        required property double textZ;

        interactive: false
        reuseItems: true

        delegate:Item {
            id: itemId

            required property string textRole
            required property point positionRole
            required property font fontRole
            required property color fillColorRole
            required property color strokeColorRole;

            Text {
                scale: gridModel.viewScale
                transformOrigin: Item.TopLeft
                text: itemId.textRole
                font: itemId.fontRole
                x: itemId.positionRole.x
                y: itemId.positionRole.y
                color: itemId.fillColorRole
            }
        }

        z: repeaterId.textZ
    }

    // component LabelView : Repeater {
    //     id: repeaterId

    //     required property double textZ;

    //     delegate:Text {
    //         scale: gridModel.viewScale
    //         transformOrigin: Item.TopLeft

    //         required property string textRole
    //         required property point positionRole
    //         required property font fontRole
    //         required property color fillColorRole
    //         required property color strokeColorRole;

    //         text: textRole
    //         font: fontRole
    //         x: positionRole.x
    //         y: positionRole.y
    //         color: fillColorRole
    //         z: repeaterId.textZ
    //     }
    // }

    //Minor grid
    LabelView {
        model: gridModel.minorTextModel
        textZ: gridModel.minorGridModel.labelZ
    }

    LabelView {
        model: gridModel.majorTextModel
        textZ: gridModel.majorGridModel.labelZ
    }

}
