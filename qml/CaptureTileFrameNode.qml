import QtQuick 2.0
import Qt3D.Core 2.1
import Qt3D.Render 2.1
import Cavewhere 1.0

/**
  This frame graph node captures one capture tile per frame. It is designed
  to use cwCaptureManager. This is used to help tile the rendering view to
  any dimension.
  */
RenderTargetSelector {

    property CaptureManager captureManager

    function requestNextCapture() {
        if(!captureManager.isScreenCaptureCommandQueueEmpty && captureConnection.command == null) {
            var command = captureManager.dequeueScreenCaptureCommand();
            forwardRendereId.camera = command.camera;
            console.log("Viewport:" + forwardRendereId.camera.viewport.width + "," + forwardRendereId.camera.viewport.height);
            var captureReply = captureId.requestCapture();
            captureConnection.command = command;
            captureConnection.target = captureReply;
            console.log("Capture!" + captureReply.complete);
        }
    }


    enabled: true

    target: RenderTarget {
        attachments: [
            RenderTargetOutput {
                objectName: "captureTexture"
                attachmentPoint: RenderTargetOutput.Color0
                texture: Texture2D {
                    id: sceneTexture

                    property var viewport: forwardRenderId.camera ? forwardRenderId.camera.viewport : null

//                    width: 2048
//                    height: 2048
                    width: viewport ? viewport.width : 1
                    height: viewport ? viewport.height : 1

                    generateMipMaps: false
                    magnificationFilter: Texture.Nearest
                    minificationFilter: Texture.Nearest
                    wrapMode {
                        x: WrapMode.ClampToEdge
                        y: WrapMode.ClampToEdge
                    }
                    comparisonFunction: Texture.CompareLessEqual
                    comparisonMode: Texture.CompareRefToTexture
                }
            },

            RenderTargetOutput {
                objectName: "depth"
                attachmentPoint: RenderTargetOutput.Depth
                texture: Texture2D {
                    id: depthTexture
                    width: sceneTexture.width
                    height: sceneTexture.height
                    format: Texture.DepthFormat
                    generateMipMaps: false
                    magnificationFilter: Texture.Nearest
                    minificationFilter: Texture.Nearest
                    wrapMode {
                        x: WrapMode.ClampToEdge
                        y: WrapMode.ClampToEdge
                    }
                    comparisonFunction: Texture.CompareLessEqual
                    comparisonMode: Texture.CompareRefToTexture
                }
            }
        ]

        //The default fraph graph nodes for cavewhere
        ForwardRenderFrameNode {
            id: forwardRenderId
            RenderCapture {
                id: captureId
            }
        }
    }

    Connections {
        target: captureManager
        onScreenCaptureCommandAdded: {
            requestNextCapture();
        }
    }

    Connections {
        id: captureConnection

        property ScreenCaptureCommand command;

        ignoreUnknownSignals: true
        onCompleteChanged: {
            var captureReply = captureConnection.target;
            console.log("Capture finished: " + captureReply.complete + " " + captureReply.captureId + " " + captureReply.image);
            console.log("Texture2D width and height:" + sceneTexture.width + "," + sceneTexture.height)
            captureReply.saveToFile("testShot" + captureReply.captureId + ".png");
            rootData.printImage(captureReply.image);
            command.image = captureReply.image;
            //            sceneRootRenderPolicy.renderPolicy = RenderSettings.OnDemand
//            capture(5);
            command = null;
            requestNextCapture();
        }
    }
}
