import QtQuick 2.0
import Qt3D.Core 2.1
import Qt3D.Render 2.1


CameraSelector {
    id: cameraSelectorId

    LayerFilter {
        objectName: "opaqueLayerFilter"
        layers: [transparentLayerId]
        filterMode: LayerFilter.DiscardAnyMatchingLayers //Only match non-transparent entity ie opaque
    }

    LayerFilter {
        objectName: "transparentLayerFilter"
        layers: [transparentLayerId]
    }
}

