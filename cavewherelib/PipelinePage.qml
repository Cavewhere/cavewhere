import cavewherelib
import QtQml
import QtQuick as QQ
import QtQuick.Layouts
import QuickQanava as Qan

StandardPage {
    id: pageId

    objectName: "dataMainPage"

    Qan.GraphView {
      id: graphView
      anchors.fill: parent
      navigable   : true
      graph: Qan.Graph {
          id: graph
          anchors.fill: parent
          Component.onCompleted: {
              var n1 = graph.insertNode()
              n1.label = "Hello World"
          }
      }
    } // Qan.GraphView

    QQ.Rectangle {
       width: 10
       height: 10
       color: "red"
    }



}
