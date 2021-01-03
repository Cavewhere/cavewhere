import QtQuick 2.2 as QQ2
import Qt3D.Core 2.1
import Qt3D.Render 2.12
import Qt3D.Input 2.1
import Qt3D.Extras 2.1

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
