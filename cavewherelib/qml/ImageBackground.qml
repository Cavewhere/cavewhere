import QtQuick as QQ

//We need to use a white background here for transparent images to look okay
QQ.Rectangle {
    id: background

    property alias image: imageId

    color: "#FFFFFF"
    width: imageId.width
    height: imageId.height

    QQ.Image {
        id: imageId
        objectName: "imageId"
        smooth: false
        mipmap: true
        fillMode: QQ.Image.Pad //No rescaling
        asynchronous: true
        visible: status === QQ.Image.Ready
    }
}
