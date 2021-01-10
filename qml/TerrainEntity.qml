import QtQuick 2.2 as QQ2
import Qt3D.Core 2.12
import Qt3D.Render 2.12
import Qt3D.Input 2.12
import Qt3D.Extras 2.12
import QtQuick.Scene2D 2.12
import QtLocation 5.14
import QtPositioning 5.12

Entity {
    id: rootId

    Entity {
        id: planeId

        PlaneMesh {
            id: plane
            width: 1
            height: 1
            meshResolution: Qt.size(10, 10)
            mirrored: true
        }

        Transform {
            id: planeTransform
            rotationX: 90
//            scale3D: Qt.vector3d(1.0, 1.0, -1.0);
            translation: Qt.vector3d(0.0, 0.0, -300);
        }

        Material {
            id: terrianMaterial

            parameters: [
                Parameter { name: "diffuseTexture"; value: offscreenTexture }
            ]

            effect: Effect {
                techniques: [
                    Technique {
                        // GL 2 Technique
                        filterKeys: [ forward ]
                        graphicsApiFilter {
                            api: GraphicsApiFilter.OpenGL
                            profile: GraphicsApiFilter.NoProfile
                            majorVersion: 2
                            minorVersion: 1
                        }

                        renderPasses: [
                            RenderPass {
                                shaderProgram: ShaderProgram {
                                    vertexShaderCode: loadSource("qrc:/shaders/simple.vert")
                                    fragmentShaderCode: loadSource("qrc:/shaders/simple.frag")
                                }

                                renderStates: [
                                    noCullingId,
                                ]
                            }
                        ]
                    }
                ]
            }
        }

        TextureMaterial {
            id: terrainTextureId
            texture: offscreenTexture
//            alphaBlending: true
        }

        components: [
            plane,
            planeTransform,
            terrianMaterial
        ]
    }

    Scene2D {
        id: qmlTexture
//        entities: [planeId]
        mouseEnabled: false

        output: RenderTargetOutput {
            attachmentPoint: RenderTargetOutput.Color0
            texture: Texture2D {
                id: offscreenTexture
                width: 2048
                height: 2048
                format: Texture.RGBA8_UNorm
                generateMipMaps: true
                magnificationFilter: Texture.Linear
                minificationFilter: Texture.LinearMipMapLinear
                wrapMode {
                    x: WrapMode.ClampToEdge
                    y: WrapMode.ClampToEdge
                }
            }
        }

        Map {
            id: mapId
            width: offscreenTexture.width
            height: offscreenTexture.height

            Plugin {
                id: mapPlugin
                name: "esri"
            }

            plugin: mapPlugin
            center: QtPositioning.coordinate(38.436962, -109.930055)
            zoomLevel: 16

            activeMapType: supportedMapTypes[1]

            onSupportedMapTypesChanged: {
               console.log("Supported MapType:");
               for (var i = 0; i < mapId.supportedMapTypes.length; i++) {
                  console.log(i, supportedMapTypes[i].name);
               }
            }

            onVisibleRegionChanged: {
                var bounding = visibleRegion.boundingGeoRectangle();
                console.debug("Visbile area:" + visibleRegion.boundingGeoRectangle())
                console.debug("topLeft:" + mapId.fromCoordinate(bounding.topLeft))
                console.debug("bottomRight:" + mapId.fromCoordinate(bounding.bottomRight))

                var verticalDistance = bounding.topLeft.distanceTo(bounding.bottomLeft);
                var horizontalDistance = bounding.topLeft.distanceTo(bounding.topRight);

                plane.width = horizontalDistance;
                plane.height = verticalDistance;
//                planeTransform.scale3D = Qt.vector3d(horizontalDistance, verticalDistance, -1.0);

                console.log("Size:" + horizontalDistance + "," + verticalDistance)

            }
        }
    }
}
