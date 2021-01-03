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
            width: 100
            height: 100
            meshResolution: Qt.size(2,2)
        }

        Transform {
            id: planeTransform
            rotationX: 90
            scale3D: Qt.vector3d(1.0, 1.0, -1.0);
            translation: Qt.vector3d(0.0, 0.0, 100);
        }

        TextureMaterial {
            id: terrainTextureId
            texture: offscreenTexture
        }

        components: [
            plane,
            planeTransform,
            terrainTextureId
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
                name: "osm" // "mapboxgl", "esri", ...

                PluginParameter {
                    name: "osm.mapping.providersrepository.disabled"
                    value: "true"
                }

                PluginParameter {
                    name: "osm.mapping.providersrepository.address"
                    value: "http://maps-redirect.qt.io/osm/5.6/"
                }
            }

            plugin: mapPlugin
            center: QtPositioning.coordinate(59.91, 10.75) // Oslo
            zoomLevel: 17

            onVisibleRegionChanged: {
                var bounding = visibleRegion.boundingGeoRectangle();
                console.debug("Visbile area:" + visibleRegion.boundingGeoRectangle())
                console.debug("topLeft:" + mapId.fromCoordinate(bounding.topLeft))
                console.debug("bottomRight:" + mapId.fromCoordinate(bounding.bottomRight))
            }
        }
    }
}
