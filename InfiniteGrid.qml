import QtQuick
import CaveWhereSketch
import cavewherelib

Item {
    id: gridId

    anchors.fill: parent

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

    // ShapePathInstantiator {
    //     anchors.fill: parent
    //     model: gridModel
    // }

    component LabelView : Repeater {
        // model: gridModel.minorTextModel
        delegate:Text {
            scale: gridModel.viewScale
            transformOrigin: Item.TopLeft

            required property string textRole
            required property point positionRole
            required property font fontRole
            required property color fillColorRole
            required property color strokeColorRole;

            text: textRole
            font: fontRole
            x: positionRole.x
            y: positionRole.y
            color: fillColorRole
        }
    }

    //Minor grid
    LabelView {
        model: gridModel.minorTextModel
    }

    LabelView {
        model: gridModel.majorTextModel
    }
}
