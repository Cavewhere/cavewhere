import QtQuick 2.2 as QQ2
import Qt3D.Core 2.1
import Qt3D.Render 2.12
import Qt3D.Input 2.1
import Qt3D.Extras 2.1
import QtQuick.Scene3D 2.0
import QtQuick.Controls 1.1
import Cavewhere 1.0 as CW

QQ2.Item {
    id: rootItem

    property alias turnTableInteraction: turnTableInteractionId
    property alias interactionManager: interactionManagerId
    property alias leadView: leadViewId
    property alias captureManager: captureTileFrameNode.captureManager
    property alias camera: cwCameraId

    clip: true

    InteractionManager {
        id: interactionManagerId
        interactions: [
            turnTableInteractionId
        ]
        defaultInteraction: turnTableInteractionId
    }

    Scene3D {
        anchors.fill: parent
        multisample: false

        Entity {
            id: sceneRoot

            components: [
                RenderSettings {
                    id: sceneRootRenderPolicy
                    renderPolicy: RenderSettings.OnDemand //RenderSettings.Always //RenderSettings.OnDemand
                    activeFrameGraph: Viewport {

                        RenderSurfaceSelector {
                            TechniqueFilter {
                                matchAll: [ FilterKey { name: "renderingStyle"; value: "forward" } ]

                                //This is to provide access to tiling the render to a raster formate (such as a PNG)
                                CaptureTileFrameNode {
                                    id: captureTileFrameNode
                                }

                                ForwardRenderFrameNode {
                                    camera: cwCameraId
                                }
                            }
                        }
                    }
                }
            ]

            CullFace {
                id: noCullingId
                mode: CullFace.NoCulling
            }

            CW.Camera {
                id: cwCameraId
                viewport: Qt.rect(0, 0, rootItem.width, rootItem.height)
                updateProjectionOnViewportChange: true
            }


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
                                    renderStates: [
                                    LineWidth {
                                            value: 5
                                        }
                                    ]
                                    shaderProgram: ShaderProgram {
                                        vertexShaderCode: loadSource("qrc:/shaders/LinePlot.vert")
                                        fragmentShaderCode: loadSource("qrc:/shaders/LinePlot.frag")
                                    }
                                }
                            ]
                        }
                    ]
                }
            }

            Effect {
                id: scrapEffect
                objectName: "scrapEffect"

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
                                    vertexShaderCode: loadSource("qrc:/shaders/scrap.vert")
                                    fragmentShaderCode: loadSource("qrc:/shaders/scrap.frag")
                                }

                                renderStates: [
                                    noCullingId
                                ]
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
            }

            Entity {
                id: grid

                PlaneMesh {
                    id: plane
                    width: 1000
                    height: 1000
                    meshResolution: Qt.size(2,2)
                }

                Transform {
                    id: planeTransform
                    rotationX: 90
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
                                            vertexShaderCode: loadSource("qrc:/shaders/grid.vert")
                                            fragmentShaderCode: loadSource("qrc:/shaders/grid.frag")
                                        }

                                        renderStates: [
                                            noCullingId
                                        ]
                                    }
                                ]
                            }
                        ]
                    }
                }

                components: [
                    plane,
                    planeTransform,
                    gridMatrial,
                ]
            }

            PhongMaterial {
                id: material
            }

            PhongMaterial {
                id: materialX
                ambient: "red"
            }

            PhongMaterial {
                id: materialY
                ambient: "green"
            }

            PhongMaterial {
                id: materialZ
                ambient: "blue"
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
                //                    scale3D: Qt.vector3d(1.5, 1, 0.5)
                //                    rotation: fromAxisAndAngle(Qt.vector3d(1, 0, 0), 45)
            }

//            Entity {
//                id: torusEntity
//                components: [ torusMesh, lineMaterial, torusTransform ]
//            }

            //                rooscrapsEntity,

            SphereMesh {
                id: sphereMesh
                radius: 3
            }

            Transform {
                id: sphereTransformX
                property real userAngle: 0.0
                matrix: {
                    var m = Qt.matrix4x4();
                    m.rotate(userAngle, Qt.vector3d(0, 1, 0));
                    m.translate(Qt.vector3d(20, 0, 0));
                    return m;
                }
            }

            Transform {
                id: sphereTransformY
                property real userAngle: 0.0
                matrix: {
                    var m = Qt.matrix4x4();
                    //                        m.rotate(userAngle, Qt.vector3d(0, 1, 0));
                    m.translate(Qt.vector3d(0, 20, 0));
                    return m;
                }
            }

            Transform {
                id: sphereTransformZ
                property real userAngle: 0.0
                matrix: {
                    var m = Qt.matrix4x4();
                    //                        m.rotate(userAngle, Qt.vector3d(0, 1, 0));
                    m.translate(Qt.vector3d(0, 0, 20));
                    return m;
                }
            }


            Transform {
                id: sphereTransformZ2
                property real userAngle: 0.0
                matrix: {
                    var m = Qt.matrix4x4();
                    //                        m.rotate(userAngle, Qt.vector3d(0, 1, 0));
                    m.translate(Qt.vector3d(0, 0, 10));
                    return m;
                }
            }

//            Entity {
//                id: sphereEntityX
//                components: [ sphereMesh, materialX, sphereTransformX, inersectorId]
//            }

//            Entity {
//                id: sphereEntityY
//                components: [ sphereMesh, materialY, sphereTransformY, inersectorId]
//            }

//            Entity {
//                id: sphereEntityZ
//                components: [ sphereMesh, materialZ, sphereTransformZ, inersectorId]
//            }

//            Entity {
//                id: sphereEntityZ2
//                components: [ sphereMesh, materialZ, sphereTransformZ2, inersectorId]
//            }

            Entity {
                id: cppEntities
            }

        }

        QQ2.Component.onCompleted: {
            rootData.scrapManager.scrapsEntity.parent = cppEntities
            rootData.scrapManager.scrapsEntity.effect = scrapEffect
        }
    }

    TurnTableInteraction {
        id: turnTableInteractionId
        anchors.fill: parent
        camera: cwCameraId
        inersecter: inersectorId
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
