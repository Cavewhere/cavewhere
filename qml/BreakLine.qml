// import QtQuick 2.0 // to target S60 5th Edition or Maemo 5
import QtQuick 2.0

Canvas {
    id: canvas
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.leftMargin: 10
    anchors.rightMargin: 10
//    border.color: "#BBBBBB"
    height: 1
    onPaint: {
        var ctx = canvas.getContext('2d');

        ctx.beginPath();
        ctx.moveTo(0.0, 0.0);
        ctx.lineTo(width, 0.0);

        ctx.stokeStyle = "#bbbbbb";
        ctx.stroke();
    }

}
