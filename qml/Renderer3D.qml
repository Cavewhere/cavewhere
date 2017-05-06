import QtQuick 2.2 as QQ2
import Qt3D.Core 2.0
import Qt3D.Render 2.0
import Qt3D.Input 2.0
import Qt3D.Extras 2.0
import QtQuick.Scene3D 2.0
import QtQuick.Controls 1.1
import Cavewhere 1.0 as CW

QQ2.Item {
    id: rootItem

    property alias turnTableInteraction: turnTableInteractionId
    property alias interactionManager: interactionManagerId
    property alias leadView: leadViewId

    clip: true

    Button {
        text: "Capture"
        z: 1
        onClicked:  {
            capture();
        }
    }

    function capture() {
        var captureReply = captureId.requestCapture(1);
        captureConnection.target = captureReply;
        console.log("Capture!" + captureReply.complete);
    }

    QQ2.Connections {
        id: captureConnection

        onCompleteChanged: {
            var captureReply = captureConnection.target;
            console.log("Capture finished: " + captureReply.complete + " " + captureReply.captureId + " " + captureReply.image);
            captureReply.saveToFile("/Users/vpicaver/Desktop/testShot.png");
            rootData.printImage(captureReply.image)
        }
    }

    CW.Camera {
        id: cwCameraId
        qt3dCamera: cameraId
        viewport: Qt.rect(0, 0, rootItem.width, rootItem.height)
    }



    InteractionManager {
        id: interactionManagerId
        interactions: [
            turnTableInteractionId
        ]
        defaultInteraction: turnTableInteractionId
    }

    Scene3D {
        anchors.fill: parent
        multisample: true

        Entity {
            id: sceneRoot

            components: [
                RenderSettings {
                    activeFrameGraph: Viewport {
                        normalizedRect: Qt.rect(0.0, 0.0, 1.0, 1.0)

                        RenderSurfaceSelector {
                            TechniqueFilter {
                                matchAll: [ FilterKey { name: "renderingStyle"; value: "forward" } ]

                                ClearBuffers {
                                    clearColor: Qt.rgba(0.0, 0.4, 0.7, 0.0);
                                    buffers: ClearBuffers.ColorDepthBuffer

                                    CameraSelector {
                                        camera: cameraId
                                        RenderCapture {
                                            id: captureId
                                        }
                                    }

                                }
                            }
                        }
                    }

                    //                    ForwardRenderer {
                    //                    clearColor: Qt.rgba(0, 0.5, 1, 1)
                    //                    camera: camera
                    //                }
                }
                // Event Source will be set by the Qt3DQuickWindow
                //                InputSettings { }
            ]



                Camera {
                    id: cameraId
                    //                projectionType: CameraLens.PerspectiveProjection
                    //                fieldOfView: 45
                    //                aspectRatio: 16/9
                    //                nearPlane : 0.1
                    //                farPlane : 1000.0
                    //                position: Qt.vector3d( 0.0, 0.0, -40.0 )
                    //                upVector: Qt.vector3d( 0.0, 1.0, 0.0 )
                    //                viewCenter: Qt.vector3d( 0.0, 0.0, 0.0 )
                }

                //            OrbitCameraController {
                //                camera: cameraId
                //            }

                FilterKey {
                    id: forward
                    name: "renderingStyle"
                    value: "forward"
                }

                CW.Inersecter {
                    id: inersectorId
                }

                Material {
                    id: lineMaterial
                    effect: Effect {

                        techniques: [
                            Technique {
                                // GL 2 Technique
                                filterKeys: [ forward ]
                                graphicsApiFilter {
                                    api: GraphicsApiFilter.OpenGL
                                    profile: GraphicsApiFilter.NoProfile
                                    majorVersion: 2
                                    minorVersion: 0
                                }

                                renderPasses: [
                                    RenderPass {
                                        shaderProgram: ShaderProgram {
                                            vertexShaderCode: loadSource("file://Users/vpicaver/Documents/Projects/cavewhere/shaders/LinePlot.vert")
                                            fragmentShaderCode: loadSource("file://Users/vpicaver/Documents/Projects/cavewhere/shaders/LinePlot.frag")
                                        }
                                    }
                                ]
                            }
                        ]
                    }
                }

                Effect {
                    id: scrapEffect

                    techniques: [
                        Technique {
                            // GL 2 Technique
                            filterKeys: [ forward ]
                            graphicsApiFilter {
                                api: GraphicsApiFilter.OpenGL
                                profile: GraphicsApiFilter.NoProfile
                                majorVersion: 2
                                minorVersion: 0
                            }

                            renderPasses: [
                                RenderPass {
                                    shaderProgram: ShaderProgram {
                                        vertexShaderCode: loadSource("file://Users/vpicaver/Documents/Projects/cavewhere/shaders/scrap.vert")
                                        fragmentShaderCode: loadSource("file://Users/vpicaver/Documents/Projects/cavewhere/shaders/scrap.frag")
                                    }
                                }
                            ]
                        }
                    ]
                }


                Entity {
                    id: linePlotEntity
                    components: [
                        rootData.renderEntity.linePlotMesh,
                        lineMaterial,
                        inersectorId
                    ]
                    //            components: [ rootData.renderEntity.linePlotMesh ]
                }

                Entity {
                    id: grid

                    PlaneMesh {
                        id: plane
                        width: 1000
                        height: 1000
                        meshResolution: qt.size(2,2)
                    }

                    Transform {
                        id: planeTransform
                        rotationX: -90
                    }

                    Material {
                        id: gridMatrial

                        parameters: [
                            Parameter { name: "model"; value: planeTransform.matrix }
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
                                                vertexShaderCode: loadSource("file://Users/vpicaver/Documents/Projects/cavewhere/shaders/grid.vert")
                                                fragmentShaderCode: loadSource("file://Users/vpicaver/Documents/Projects/cavewhere/shaders/grid.frag")
                                            }
                                        }
                                    ]
                                }
                            ]
                        }
                    }

                    components: [
                        plane,
                        planeTransform,
                        gridMatrial
                    ]
                }

                PhongMaterial {
                    id: material
                }

                TorusMesh {
                    id: torusMesh
                    radius: 5
                    minorRadius: 1
                    rings: 100
                    slices: 20
                }

                Transform {
                    id: torusTransform
                    scale3D: Qt.vector3d(1.5, 1, 0.5)
                    rotation: fromAxisAndAngle(Qt.vector3d(1, 0, 0), 45)
                }

                Entity {
                    id: torusEntity
                    components: [ torusMesh, material, torusTransform ]
                }

                //                rooscrapsEntity,

                SphereMesh {
                    id: sphereMesh
                    radius: 3
                }

                Transform {
                    id: sphereTransform
                    property real userAngle: 0.0
                    matrix: {
                        var m = Qt.matrix4x4();
                        m.rotate(userAngle, Qt.vector3d(0, 1, 0));
                        m.translate(Qt.vector3d(20, 0, 0));
                        return m;
                    }
                }

                Entity {
                    id: sphereEntity
                    components: [ sphereMesh, material, sphereTransform ]
                }




//                QQ2.NumberAnimation {
//                    target: sphereTransform
//                    property: "userAngle"
//                    duration: 10000
//                    from: 0
//                    to: 360

//                    loops: QQ2.Animation.Infinite
//                    running: false
//                }

                Entity {
                    id: cppEntities
//                    data: [
//                        torusEntity2
////                        rootData.scrapManager.scrapsEntity
//                    ]
                }

        }

        QQ2.Component.onCompleted: {
            rootData.scrapManager.scrapsEntity.parent = cppEntities
            rootData.scrapManager.scrapsEntity.effect = scrapEffect
        }
    }

    Entity {
        id: torusEntity2
        components: [ torusMesh, lineMaterial]
        parent: cppEntities
    }


    TurnTableInteraction {
        id: turnTableInteractionId
        anchors.fill: parent
        camera: cwCameraId
        inersecter: inersectorId

        //        scene: renderer.scene
    }

    CW.LinePlotLabelView {
        id: labelView
        anchors.fill: parent
        camera: cwCameraId
        region: rootData.region
    }

    CW.LeadView {
        id: leadViewId
        anchors.fill: parent
        regionModel: rootData.regionTreeModel
        camera: cwCameraId
        visible: rootData.leadsVisible
    }


}
