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
        model: gridModel
    }

    //Label Repeater for the grid labels
    Repeater {
        model: gridModel.textModel
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



    // FixedGridModel {
    //     id: grid1
    //     viewport: containerId.viewport
    //     // origin: minorGridModel.origin

    //     lineColor: "#1eb6dd"
    //     labelColor: "#178ba8"
    //     labelScale:  //This keeps the labels visible
    //     lineWidth: Math.max(1.0, 1.0 * labelScale)


    //     gridInterval.value: 10.0
    //     // gridInterval.unit: Units.Feet
    //     // mapMatrix: worldToScreenId.matrix
    //     gridVisible: msaaGridVisible(lineWidth)
    // }

    // FixedGridModel {
    //     id: grid2
    //     viewport: grid1.viewport
    //     origin: grid1.origin
    //     mapMatrix: grid1.mapMatrix

    //     lineColor: "#5dcae7"
    //     lineWidth: Math.max(0.5, 0.5 * grid1.labelScale)

    //     labelVisible: false
    //     labelScale: grid1.labelScale

    //     gridVisible: msaaGridVisible(lineWidth)
    //     gridInterval.value: 1.0
    // }

    // ShapePathInstantiator {
    //     anchors.fill: parent
    //     model: grid2
    // }

    // ShapePathInstantiator {
    //     anchors.fill: parent
    //     model: grid1
    // }

    // //Label Repeater for the grid labels
    // Repeater {
    //     model: grid1.textModel
    //     delegate:Text {
    //         scale: grid2.labelScale
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
    //     }
    // }
}
