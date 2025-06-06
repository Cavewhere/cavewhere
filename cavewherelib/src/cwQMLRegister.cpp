/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwQMLRegister.h"


//Qt registeration
#include <QQuickView>
#include <QScreen>
#include <QGraphicsScene>
#include <QDebug>
#include <QUndoStack>
#include <QThread>
#include <QProcess>

cwQMLRegister::cwQMLRegister()
{
}

// void cwQMLRegister::registerQML()
// {
    // qRegisterMetaType<QThread*>("QThread*");
    // qRegisterMetaType<cwCavingRegion>("cwCavingRegion");
    // qRegisterMetaType<QList <QString> >("QList<QString>");
    // qRegisterMetaType<QList <cwImage> >("QList<cwImage>");
    // qRegisterMetaType<QList <cwStation > >("QList<cwStation>");
    // qRegisterMetaType<QList <cwPage* > >("QList<cwPage*>");
    // qRegisterMetaType<QList <cwCave* > >("QList<cwCave*>");
    // qRegisterMetaType<QModelIndex>("QModelIndex");
    // qRegisterMetaType<cwImage>("cwImage");
    // qRegisterMetaType<GLuint>("GLuint");
    // qRegisterMetaType<cwUsedStationsTask::Settings>("cwUsedStationsTask::Settings");

    // qmlRegisterType<cwCavingRegion>("Cavewhere", 1, 0, "CavingRegion");
    // qmlRegisterType<cwCave>("Cavewhere", 1, 0, "Cave");
    // qmlRegisterType<cwSurveyChunk>("Cavewhere", 1, 0, "SurveyChunk");
    // qmlRegisterType<cwSurveyChunkView>("Cavewhere", 1, 0, "SurveyChunkView");
    // qmlRegisterType<cwSurveyChunkGroupView>("Cavewhere", 1, 0, "SurveyChunkGroupView");
    // qmlRegisterType<cwTrip>("Cavewhere", 1, 0, "Trip");
    // qmlRegisterType<cwValidator>("Cavewhere", 1, 0, "Validator");
    // qmlRegisterType<cwClinoValidator>("Cavewhere", 1, 0, "ClinoValidator");
    // qmlRegisterType<cwStationValidator>("Cavewhere", 1, 0, "StationValidator");
    // qmlRegisterType<cwCompassValidator>("Cavewhere", 1, 0, "CompassValidator");
    // qmlRegisterType<cwDistanceValidator>("Cavewhere", 1, 0, "DistanceValidator");
    // qmlRegisterType<cwSurveyNoteModel>("Cavewhere", 1, 0, "NoteModel");
    // qmlRegisterType<cwRegionTreeModel>("Cavewhere", 1, 0, "RegionTreeModel");
    // qmlRegisterType<cwUsedStationTaskManager>("Cavewhere", 1, 0, "UsedStationTaskManager");
    // // qmlRegisterType<cw3dRegionViewer>("Cavewhere", 1, 0, "RegionViewer");
    // qmlRegisterType<cwLinePlotManager>("Cavewhere", 1, 0, "LinePlotManager");
    // qmlRegisterType<cwGLLinePlot>("Cavewhere", 1, 0, "GLLinePlot");
    // qmlRegisterType<cwProject>("Cavewhere", 1, 0, "Project");
    // qmlRegisterType<cwNote>("Cavewhere", 1, 0, "Note");
    // qmlRegisterType<cwBasePanZoomInteraction>("Cavewhere", 1, 0, "BasePanZoomInteraction");
    // qmlRegisterType<cwBaseScrapInteraction>("Cavewhere", 1, 0, "BaseScrapInteraction");
    // qmlRegisterType<cwCamera>("Cavewhere", 1, 0, "Camera");
    // qmlRegisterType<cwImageItem>("Cavewhere", 1, 0, "ImageItem");
    // qmlRegisterType<cwScrapView>("Cavewhere", 1, 0, "ScrapView");
    // qmlRegisterType<cwTransformUpdater>("Cavewhere", 1, 0, "TransformUpdater");
    // qmlRegisterType<cwScrapStationView>("Cavewhere", 1, 0, "ScrapStationView");
    // qmlRegisterType<cwScrap>("Cavewhere", 1, 0, "Scrap");
    // qmlRegisterType<cwBaseNoteStationInteraction>("Cavewhere", 1, 0, "BaseNoteStationInteraction");
    // qmlRegisterType<cwNoteTranformation>("Cavewhere", 1, 0, "NoteTransform");
    // qmlRegisterType<cwScrapItem>("Cavewhere", 1, 0, "ScrapItem");
    // qmlRegisterType<cwLength>("Cavewhere", 1, 0, "Length");
    // qmlRegisterType<cwInteraction>("Cavewhere", 1, 0, "Interaction");
    // qmlRegisterType<cwNorthArrowItem>("Cavewhere", 1, 0, "NorthArrowItem");
    // qmlRegisterType<cwBasePositioner>("Cavewhere", 1, 0, "BasePositioner");
    // qmlRegisterAnonymousType<cwAbstract2PointItem>("Cavewhere", 1);
    // qmlRegisterType<cwScaleLengthItem>("Cavewhere", 1 ,0, "ScaleLengthItem");
    // qmlRegisterType<cwImageProperties>("Cavewhere", 1, 0, "ImageProperties");
    // qmlRegisterType<cwUnits>("Cavewhere", 1, 0, "Units");
    // qmlRegisterType<cwGLScraps>("Cavewhere", 1, 0, "GLScraps");
    // qmlRegisterType<cwScrapManager>("Cavewhere", 1, 0, "ScrapManager");
    // qmlRegisterType<cwTeam>("Cavewhere", 1, 0, "Team");
    // qmlRegisterType<cwTripCalibration>("Cavewhere", 1, 0, "Calibration");
    // qmlRegisterType<cwSurveyChunkTrimmer>("Cavewhere", 1, 0, "SurveyChunkTrimmer");
    // qmlRegisterType<cwItemSelectionModel>("Cavewhere", 1, 0, "ItemSelectionModel");
    // qmlRegisterType<cwSurveyExportManager>("Cavewhere", 1, 0, "SurveyExportManager");
    // qmlRegisterType<cwSurveyImportManager>("Cavewhere", 1, 0, "SurveyImportManager");
    // qmlRegisterType<cwTripLengthTask>("Cavewhere", 1, 0, "TripLengthTask");
    // qmlRegisterType<cwLabel3dView>("Cavewhere", 1, 0, "Label3dView");
    // qmlRegisterType<cwLinePlotLabelView>("Cavewhere", 1, 0, "LinePlotLabelView");
    // qmlRegisterType<cwScrapItem>("Cavewhere", 1, 0, "ScrapItem");
    // qmlRegisterAnonymousType<cwAbstractPointManager>("Cavewhere", 1);
    // qmlRegisterType<cwScrapOutlinePointView>("Cavewhere", 1, 0, "ScrapControlPointView");
    // qmlRegisterType<cwSelectionManager>("Cavewhere", 1, 0, "SelectionManager");
    // qmlRegisterAnonymousType<cwUnitValue>("Cavewhere", 1);
    // qmlRegisterType<cwImageResolution>("Cavewhere", 1, 0, "ImageResolution");
    // qmlRegisterType<cwSceneToImageTask>("Cavewhere", 1, 0, "ExportRegioonViewerToImageTask");
    // qmlRegisterType<cwOrthogonalProjection>("Cavewhere", 1, 0, "OrthogonalProjection");
    // qmlRegisterType<cwPerspectiveProjection>("Cavewhere", 1, 0, "PerspectiveProjection");
    // qmlRegisterType<cwMatrix4x4Animation>("Cavewhere", 1, 0, "Matrix4x4Animation");
    // qmlRegisterType<cwImageValidator>("Cavewhere", 1, 0, "ImageValidator");
    // qmlRegisterType<cwQMLReload>("Cavewhere", 1, 0, "QMLReload");
    // qmlRegisterType<cwCompassItem>("Cavewhere", 1, 0, "CompassItem");
    // qmlRegisterType<cwShaderDebugger>("Cavewhere", 1, 0, "ShaderDebugger");
    // qmlRegisterType<cwLicenseAgreement>("Cavewhere", 1, 0, "LicenseAgreement");
    // qmlRegisterType<cwRegionSceneManager>("Cavewhere", 1, 0, "RegionSceneManager");
    // qmlRegisterType<cwCaptureManager>("Cavewhere", 1, 0, "CaptureManager");
    // qmlRegisterType<cwScene>("Cavewhere", 1, 0, "Scene");
    // qmlRegisterType<cwRhiViewer>("Cavewhere", 1, 0, "RhiViewer");
    // qmlRegisterType<QQuickView>("Cavewhere", 1, 0, "QQuickView");
    // qmlRegisterAnonymousType<QScreen>("Cavewhere", 1);
    // qmlRegisterType<cwScale>("Cavewhere", 1, 0, "Scale");
    // // qmlRegisterType<cwBaseTurnTableInteraction>("Cavewhere", 1, 0, "BaseTurnTableInteraction");
    // qmlRegisterType<cwCaptureViewport>("Cavewhere", 1, 0, "CaptureViewport");
    // qmlRegisterType<cwCaptureItem>("Cavewhere", 1, 0, "CaptureItem");
    // qmlRegisterType<cwQuickSceneView>("Cavewhere", 1, 0, "QuickSceneView");
    // qmlRegisterType<QGraphicsScene>("Cavewhere", 1, 0, "GraphicsScene");
    // qmlRegisterType<cwCaptureItemManiputalor>("Cavewhere", 1, 0, "CaptureItemManiputalor");
    // qmlRegisterType<cwCaptureGroupModel>("Cavewhere", 1, 0, "CaptureGroupModel");
    // qmlRegisterType<cwTaskManagerModel>("Cavewhere", 1, 0, "TaskManagerModel");
    // qmlRegisterType<cwPageSelectionModel>("Cavewhere", 1, 0, "PageSelectionModel");
    // qmlRegisterType<cwPageView>("Cavewhere", 1, 0, "PageView");
    // qmlRegisterType<cwPage>("Cavewhere", 1, 0, "Page");
    // qmlRegisterType<QUndoStack>("Cavawhere", 1, 0, "UndoStack");
    // qmlRegisterAnonymousType<cwPageViewAttachedType>("Cavewhere", 1);
    // qmlRegisterAnonymousType<cwBaseNotePointInteraction>("Cavewhere", 1);
    // qmlRegisterType<cwBaseNoteLeadInteraction>("Cavewhere", 1, 0, "BaseNoteLeadInteraction");
    // qmlRegisterType<cwScrapLeadView>("Cavewhere", 1, 0, "ScrapLeadView");
    // qmlRegisterAnonymousType<cwScrapPointView>("Cavewhere", 1);
    // qmlRegisterType<cwRegionTreeModel>("Cavewhere", 1, 0, "RegionTreeModel");
    // qmlRegisterType<cwLeadView>("Cavewhere", 1, 0, "LeadView");
    // qmlRegisterType<cwLinkGenerator>("Cavewhere", 1, 0, "LinkGenerator");
    // qmlRegisterType<cwLeadModel>("Cavewhere", 1, 0, "LeadModel");
    // qmlRegisterType<cwSortFilterProxyModel>("Cavewhere", 1, 0, "SortFilterProxyModel");
    // qmlRegisterType<cwLeadsSortFilterProxyModel>("Cavewhere", 1, 0, "LeadsSortFilterProxyModel");
    // qmlRegisterType<cwLinkBarModel>("Cavewhere", 1, 0, "LinkBarModel");
    // qmlRegisterType<cwErrorModel>("Cavewhere", 1, 0, "ErrorModel");
    // qmlRegisterType<cwErrorListModel>("Cavewhere", 1, 0, "ErrorListModel");
    // qmlRegisterUncreatableType<cwError>("Cavewhere", 1, 0, "cwError", "Should only be created by cwSurveyChunk");

    // qmlRegisterType<cwTestcaseManager>("Cavewhere", 1, 0, "TestcaseManager");
    // qmlRegisterType<cwCSVImporterManager>("Cavewhere", 1, 0, "CSVImporterManager");
    // qmlRegisterType<cwColumnNameModel>("Cavewhere", 1, 0, "ColumnNameModel");
    // qmlRegisterType<cwCSVLineModel>("Cavewhere", 1, 0, "CSVLineModel");
    // qmlRegisterType<cwFutureFilterModel>("Cavewhere", 1, 0, "FutureFilterModel");
    // qmlRegisterType<cwFutureManagerModel>("Cavewhere", 1, 0, "FutureManagerModel");
    // qmlRegisterType<cwTaskFutureCombineModel>("Cavewhere", 1, 0, "TaskFutureCombineModel");
    // qmlRegisterUncreatableType<cwOpenGLSettings>("Cavewhere", 1, 0, "OpenGLSettings", "Should only be created in cwSettings also is static across the application");
    // qmlRegisterSingletonType( QUrl("qrc:/qml/UnitDefaults.qml"), "Cavewhere", 1, 0, "UnitDefaults");
    // qmlRegisterUncreatableType<cwSettings>("Cavewhere", 1, 0, "Settings", "Should only be created in cwRootData also is static across the application");
    // qmlRegisterUncreatableType<cwJobSettings>("Cavewhere", 1, 0, "JobSettings", "Should only be created in cwSettings also is static across the application");
    // qmlRegisterUncreatableType<cwPDFSettings>("Cavewhere", 1, 0, "PDFSettings", "Should only be created in cwSettings also is static across the application");

    // qmlRegisterAnonymousType<cwAbstractScrapViewMatrix>("Cavewhere", 1);
    // qmlRegisterType<cwPlanScrapViewMatrix>("Cavewhere", 1, 0, "PlanScrapViewMatrix");
    // qmlRegisterType<cwRunningProfileScrapViewMatrix>("Cavewhere", 1, 0, "RunningProfileScrapViewMatrix");
    // qmlRegisterType<cwProjectedProfileScrapViewMatrix>("Cavewhere", 1, 0, "ProjectedProfileScrapViewMatrix");




// }
