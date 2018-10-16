import QtQuick 2.0
import Qt3D.Core 2.1
import Qt3D.Render 2.1

ClearBuffers {
    property alias camera: cameraSelectorId.camera
    default property alias cameraChildren: cameraSelectorId.childNodes

    clearColor: Qt.rgba(0.0, 0.0, 0.0, 0.0);
    buffers: ClearBuffers.ColorDepthBuffer

    CameraSelector {
        id: cameraSelectorId
    }
}
