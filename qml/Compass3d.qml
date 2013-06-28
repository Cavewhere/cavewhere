import Cavewhere 1.0
import QtQuick 2.0
import Qt3D 2.0
import Qt3D.Shapes 2.0

Viewport {
    id: viewportId
    property var rotation

    width: 100
    height: 100

    lightModel: null

    //Disable user interaction from the user
    navigation: false

    camera: Camera {
        eye: Qt.vector3d(0, 0, 10)
        projectionType: Camera.Orthographic
    }

    Item3D {
        mesh: Mesh { source: "qrc:/3dModels/compass.3ds" }

        scale: 0.05

        transform: [
            Rotation3D {
                angle: 90
                axis: Qt.vector3d(1,0,0)
            },

            QuaternionRotation3d {
                id: penguinTilt
                quaternion: viewportId.rotation
            },

            Rotation3D {
                angle: -90
                axis: Qt.vector3d(1,0,0)
            }
        ]

    }
}
