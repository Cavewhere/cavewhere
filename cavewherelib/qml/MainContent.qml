pragma ComponentBehavior: Bound
import QtQuick as QQ
import cavewherelib

QQ.Item {
    id: mainContentId

    property GLTerrainRenderer renderer;

    anchors.fill: parent

    LinkBar {
        id: linkBar

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right

        sidebarWidth: mainSideBar.width - 1
    }

    MainSideBar {
        id: mainSideBar;
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: 0
        anchors.top: linkBar.bottom

        //For animating which page is shown
        property real pageShownReal: pageShown;

        QQ.Behavior on pageShownReal {
            QQ.NumberAnimation {
                duration: 150
            }
        }
    }

    QQ.Item {
        id: container;
        anchors.top: linkBar.bottom
        anchors.bottom: parent.bottom
        anchors.left: mainSideBar.right
        anchors.right: parent.right

        property int currentPosition: height * mainSideBar.pageShownReal

        PageView {
            id: pageView
            anchors.fill: parent
            pageSelectionModel: RootData.pageSelectionModel

            QQ.Component.onCompleted: {
                RootData.pageView = pageView
            }
        }
    }

    QQ.Item {
        id: overlay
        anchors.fill: parent
    }

    QQ.Component {
        id: renderingComponent
        RenderingView {
            width:  parent.width
            height: parent.height
            x: 0; y: -container.currentPosition
            scene: RootData.regionSceneManager.scene
        }
    }

    QQ.Component {
        id: dataMainPageComponent
        DataMainPage {
            width:  parent.width
            height: parent.height
            x: 0;
            y: height - container.currentPosition
        }
    }

    QQ.Component {
        id: mapPageComponent
        MapPage {
            width:  parent.width
            height: parent.height
            view: mainContentId.renderer;
        }
    }

    QQ.Component {
        id: unknownPageComponent
        UnknownPage {
            anchors.fill: parent
        }
    }

    QQ.Component {
        id: testcasesPageComponent
        TestcasePage {
            anchors.fill: parent
        }
    }

    QQ.Component {
        id: aboutPageComponent
        AboutPage {
            anchors.fill: parent
        }
    }

    QQ.Component {
        id: settingsPageComponent
        SettingsPage {
            anchors.fill: parent
        }
    }

    QQ.Component {
        id: pipelinePageComponent
        PipelinePage {
            anchors.fill: parent
        }
    }

    QQ.Component.onCompleted: {
        GlobalShadowTextInput.parent = overlay;
        RootPopupItem.parent = overlay

        pageView.unknownPageComponent = unknownPageComponent
        let viewPage = RootData.pageSelectionModel.registerPage(null, "View", renderingComponent);
        let dataPage = RootData.pageSelectionModel.registerPage(null, "Data", dataMainPageComponent);
        let mapPage = RootData.pageSelectionModel.registerPage(null, "Map", mapPageComponent)
        RootData.pageSelectionModel.registerPage(null, "Testcases", testcasesPageComponent);
        RootData.pageSelectionModel.registerPage(null, "About", aboutPageComponent)
        RootData.pageSelectionModel.registerPage(null, "Settings", settingsPageComponent)
        RootData.pageSelectionModel.registerPage(null, "Pipeline", pipelinePageComponent)
        RootData.pageSelectionModel.gotoPage(viewPage);

        mainContentId.renderer = pageView.pageItem(viewPage).renderer;
    }
}
