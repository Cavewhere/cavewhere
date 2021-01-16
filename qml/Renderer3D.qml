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

                                ClearBuffers {
                                    clearColor: Qt.rgba(0.0, 0.0, 0.0, 0.0);
                                    buffers: ClearBuffers.ColorDepthBuffer
                                    NoDraw {}
                                }

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

            Layer {
                id: transparentLayerId
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
                                            value: 1
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

            TerrainEntity {}

            GridPlaneEntity {}

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
