pragma ComponentBehavior: Bound
import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

QQ.Item {
    id: mainContentId

    property GLTerrainRenderer renderer;
    property QC.Menu fileMenu
    property AskToSaveDialog askToSaveDialog: null

    // Page addresses — set in onCompleted after page registration
    property string viewPageAddress
    property string dataPageAddress
    property string mapPageAddress

    readonly property int layoutSize: {
        if (width >= Theme.breakpointWide) return Theme.LayoutSize.Wide
        if (width >= Theme.breakpointMedium) return Theme.LayoutSize.Medium
        return Theme.LayoutSize.Narrow
    }

    // Whether the sidebar is on screen, which is what every floating surface
    // here is really asking: the sidebar-hinged ones belong to the widths that
    // have one, the phone's task sheet to the widths that do not. Named once
    // because the two have to be exact complements — a surface built for a width
    // it cannot be reached at is one nobody can close.
    readonly property bool sidebarShown: mainContentId.layoutSize >= Theme.LayoutSize.Medium

    anchors.fill: parent

    LinkBar {
        id: linkBar

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right

        layoutSize: mainContentId.layoutSize
        sidebarWidth: mainContentId.sidebarShown ? mainSideBar.width - 1 : 0
        viewPageAddress: mainContentId.viewPageAddress
        dataPageAddress: mainContentId.dataPageAddress
        mapPageAddress: mainContentId.mapPageAddress

        onTasksRequested: {
            if (taskSheetLoader.item) {
                taskSheetLoader.item.toggle()
            }
        }
    }

    MainSideBar {
        id: mainSideBar;
        visible: mainContentId.sidebarShown
        layoutSize: mainContentId.layoutSize
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: 0
        anchors.top: linkBar.bottom
        fileMenu: mainContentId.fileMenu

        tasksShown: taskFlyoutLoader.item ? taskFlyoutLoader.item.shown : false

        onTasksRequested: {
            if (taskFlyoutLoader.item) {
                taskFlyoutLoader.item.togglePin()
            }
        }

        // //For animating which page is shown
        // property real pageShownReal: pageShown;

        // QQ.Behavior on pageShownReal {
        //     QQ.NumberAnimation {
        //         duration: 150
        //     }
        // }
    }

    QQ.Item {
        id: container;
        anchors.top: linkBar.bottom
        anchors.bottom: parent.bottom
        anchors.left: mainContentId.sidebarShown ? mainSideBar.right : parent.left
        anchors.right: parent.right

        // property int currentPosition: height * mainSideBar.pageShownReal

        PageView {
            id: pageView
            anchors.fill: parent
            pageSelectionModel: RootData.pageSelectionModel

            QQ.Component.onCompleted: {
                RootData.pageView = pageView
            }
        }
    }

    // The active page's tool options, hinged to the sidebar's right edge and
    // drawn above the page view. It reads the same ActiveTools contract the
    // sidebar tool rail does (tools + interactionManager) and shows only while an
    // armed tool has options. Kept a sibling after `container` so it composites
    // above the 3D view; a child of MainSideBar could not, since z can't lift it
    // past a sibling Item.
    ToolPropertyFlyout {
        id: toolPropertyFlyout
        anchors.left: mainSideBar.right
        anchors.leftMargin: Theme.toolFlyoutGap
        anchors.verticalCenter: container.verticalCenter

        hostVisible: mainContentId.sidebarShown
        interactionManager: ActiveTools.interactionManager
        toolModel: ActiveTools.tools
    }

    // What the sidebar footer's busy row is busy with. Bottom-aligned to the
    // sidebar so it reads as hinged to the footer that opened it, and a sibling
    // after `container` for the same compositing reason as the tool flyout.
    //
    // Kept out of existence until there is something to list and a sidebar to
    // hinge it to, rather than merely hidden: the card's list binds to the live
    // task model, so an idle instance still carries a delegate per running job
    // and re-runs their progress bindings behind a card nobody can see.
    // ShutdownScreen's copy of the same list is Loader-gated for the same reason.
    QQ.Loader {
        id: taskFlyoutLoader
        anchors.left: mainSideBar.right
        anchors.leftMargin: Theme.toolFlyoutGap
        anchors.bottom: mainSideBar.bottom
        anchors.bottomMargin: Theme.toolFlyoutGap

        active: ActiveTasks.count > 0 && mainContentId.sidebarShown

        sourceComponent: TaskFlyout {
            objectName: "taskFlyout"

            hostVisible: mainContentId.sidebarShown
            previewHovered: mainSideBar.busyRowHovered
        }
    }

    // The same list, for the widths where the sidebar and its flyout do not
    // exist. Opened by the top bar's status chip, and a sibling after `container`
    // for the same compositing reason as the two flyouts — its scrim has to
    // cover the page view.
    //
    // Kept out of existence until there is something to list, for the same
    // reason as the task flyout: a hidden instance still carries a delegate per
    // running job. Gated on the sidebar being away as well, which is also what
    // dismisses it on a widen — the chip that opens it is gone at those widths,
    // so a sheet left latched open would have nothing to close it with.
    QQ.Loader {
        id: taskSheetLoader
        anchors.fill: container

        active: ActiveTasks.count > 0 && !mainContentId.sidebarShown

        sourceComponent: TaskSheet {
            objectName: "taskSheet"

            automaticUpdate: RootData.updateCoordinator.automaticUpdate

            onAutomaticUpdateToggled: (enabled) => RootData.updateCoordinator.automaticUpdate = enabled
        }
    }

    QQ.Item {
        id: overlay
        anchors.fill: parent
    }

    QQ.Component {
        id: renderingComponent
        RenderingView {
            anchors.fill: parent
            scene: RootData.regionSceneManager.scene
            // width:  parent.width
            // height: parent.height
            // x: 0; y: -container.currentPosition
        }
    }

    QQ.Component {
        id: dataMainPageComponent
        DataMainPage {
            anchors.fill: parent
            // width:  parent.width
            // height: parent.height
            // x: 0;
            // y: height - container.currentPosition
        }
    }

    QQ.Component {
        id: sourceComponent
        SourceListPage {
            anchors.fill: parent
            askToSaveDialog: mainContentId.askToSaveDialog
        }
    }

    QQ.Component {
        id: mapPageComponent
        MapPage {
            anchors.fill: parent
            view: mainContentId.renderer;
            // width:  parent.width
            // height: parent.height
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
        id: colorsPageComponent
        ColorsPage {
            anchors.fill: parent
        }
    }

    QQ.Component {
        id: remoteRepositoryPageComponent
        RemoteRepositoryPage {
            anchors.fill: parent
            askToSaveDialog: mainContentId.askToSaveDialog
        }
    }

    QQ.Component {
        id: pipelinePageComponent
        PipelinePage {
            anchors.fill: parent
        }
    }

    QQ.Component {
        id: remoteManagementPageComponent
        RemoteManagementPage {
            anchors.fill: parent
        }
    }

    QQ.Component {
        id: gitHistoryPageComponent
        GitHistoryPage {
            anchors.fill: parent
        }
    }

    QQ.Component {
        id: cavernOutputPageComponent
        CavernOutputPage {
            anchors.fill: parent
        }
    }

    QQ.Component {
        id: docsPageComponent
        DocsPage {
            anchors.fill: parent
        }
    }

    QQ.Component.onCompleted: {
        GlobalShadowTextInput.parent = overlay;
        RootPopupItem.parent = overlay

        pageView.unknownPageComponent = unknownPageComponent
        let viewPage = RootData.pageSelectionModel.registerPage(null, "View", renderingComponent);
        let repositoryPage = RootData.pageSelectionModel.registerPage(null, "Source", sourceComponent);
        RootData.pageSelectionModel.registerPage(null, "Remote", remoteRepositoryPageComponent);
        let dataPage = RootData.pageSelectionModel.registerPage(repositoryPage, "Data", dataMainPageComponent);
        let mapPage = RootData.pageSelectionModel.registerPage(null, "Map", mapPageComponent)
        RootData.pageSelectionModel.registerPage(null, "Testcases", testcasesPageComponent);
        RootData.pageSelectionModel.registerPage(null, "About", aboutPageComponent)
        RootData.pageSelectionModel.registerPage(null, "Settings", settingsPageComponent)
        RootData.pageSelectionModel.registerPage(null, "Colors", colorsPageComponent)
        RootData.pageSelectionModel.registerPage(null, "Pipeline", pipelinePageComponent)
        RootData.pageSelectionModel.registerPage(null, "Remote Settings", remoteManagementPageComponent)
        RootData.pageSelectionModel.registerPage(null, "History", gitHistoryPageComponent)
        RootData.pageSelectionModel.registerPage(null, "Cavern", cavernOutputPageComponent)

        // The Docs landing page (empty slug shows the manual index); every manual
        // article is a child page sharing the one DocsPage component, selected by
        // its slug. Explicit component + props — never registerSubPage.
        let docsPage = RootData.pageSelectionModel.registerPage(null, "Docs", docsPageComponent, {slug: ""})
        let manualArticles = RootData.manualIndex.articles
        for (let i = 0; i < manualArticles.length; i++) {
            let article = manualArticles[i]
            RootData.pageSelectionModel.registerPage(docsPage, article.slug, docsPageComponent,
                                                     {slug: article.slug, docsRootPage: docsPage})
        }

        mainSideBar.viewPage = viewPage;
        mainSideBar.dataPage = dataPage;
        mainSideBar.mapPage = mapPage;

        mainContentId.viewPageAddress = viewPage.fullname();
        mainContentId.dataPageAddress = dataPage.fullname();
        mainContentId.mapPageAddress = mapPage.fullname();

        if(RootData.desktopBuild) {
            RootData.pageSelectionModel.gotoPage(viewPage);
            mainContentId.renderer = pageView.pageItem(viewPage).renderer;
        } else {
            RootData.pageSelectionModel.gotoPage(repositoryPage);
        }
    }
}
