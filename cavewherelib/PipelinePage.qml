import cavewherelib
import QtQml
import QtQuick as QQ
import QtQuick.Layouts
import QuickQanava as Qan

StandardPage {
    id: pageId

    objectName: "dataMainPage"
    clip: true

    PipelineManager {
        id: pipelineId

        graph: graphId

        // graph.containerItem.parent: graphView

        Component.onCompleted: {
            pipelineId.addArtifact(surveyDataId);
            pipelineId.addArtifact(survexInputFilename);
            pipelineId.addRule(survexExporterRuleId);
            pipelineId.addRule(surveyNetworkBuilderRuleId);

            // let test = pipelineId.graph.insertNode()
              // test.label = "Test 2";

              // console.log("Pipeline graph:" + test + pipelineId.graph.getNodeCount())
        }
    }

    TemporaryFileNameArtifact {
          id: survexInputFilename
          suffix: "svx"
    }

    SurveyDataArtifact {
          id: surveyDataId
          region: RootData.region
    }

    SurvexExporterRule {
          id: survexExporterRuleId
    }

    SurveyNetworkBuilderRule {
        id: surveyNetworkBuilderRuleId
    }

    // Qan.Graph {
    //     id: graph2Id
    //     anchors.fill: parent
    //     Component.onCompleted: {
    //         var n1 = graph2Id.insertNode()
    //         n1.label = "Hello World2"
    //     }
    // }

    Qan.GraphView {
      id: graphView
      anchors.fill: parent
      navigable: true
      // graph: pipelineId.graph

      // Component.onCompleted: {
      //       pipelineId.graph.parent = graphView
      // }

      // graph: pipelineId.graph

      graph: Qan.Graph {
          id: graphId
          anchors.fill: parent


          // connectorEnabled: true
          // // connectorEdgeInserted: console.debug( "Edge inserted between " + src.label + " and  " + dst.label)
          // connectorEdgeColor: "violet"
          // connectorColor: "lightblue"
      }
    } // Qan.GraphView

}
