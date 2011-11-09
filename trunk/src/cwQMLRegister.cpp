#include "cwQMLRegister.h"
#include "cwSurveyChunk.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwSurvexImporter.h"
#include "cwTrip.h"
#include "cwSurveyChunkView.h"
#include "cwSurveyChunkGroupView.h"
#include "cwClinoValidator.h"
#include "cwStationValidator.h"
#include "cwCompassValidator.h"
#include "cwDistanceValidator.h"
#include "cwSurveyNoteModel.h"
#include "cwNoteItem.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwRegionTreeModel.h"
#include "cwImportSurvexDialog.h"
#include "cwTreeView.h"
#include "cwQMLWidget.h"
#include "cwSurvexExporterTripTask.h"
#include "cwSurvexExporterCaveTask.h"
#include "cwSurvexExporterRegionTask.h"
#include "cwCompassExporterCaveTask.h"
#include "cwLinePlotManager.h"
#include "cwUsedStationTaskManager.h"
#include "cwGlobalUndoStack.h"
#include "cwGLRenderer.h"
#include "cwGLLinePlot.h"
#include "cwSurveyNoteModel.h"
#include "cw3dRegionViewer.h"
#include "cwProject.h"
#include "cwImageDatabase.h"
#include "cwFileDialogHelper.h"
#include "cwProjectImageProvider.h"
#include "cwXMLProjectLoadSaveTask.h"
#include "cwBasePanZoomInteraction.h"
#include "cwBaseScrapInteraction.h"
#include "cwCamera.h"
#include "cwImageItem.h"
#include "cwWheelArea.h"
#include "cwScrapView.h"
#include "cwTransformUpdater.h"
#include "cwBaseNoteStationInteraction.h"
#include "cwScrap.h"
#include "cwNoteStationView.h"
#include "cwScrapStationView.h"
#include "cwNoteTranformation.h"
#include "cwScrapItem.h"
#include "cwLength.h"
#include "cwInteraction.h"
//#include "cwInteractionManager.h"

cwQMLRegister::cwQMLRegister()
{
}

void cwQMLRegister::registerQML()
{
    const char* uri = "Cavewhere";
    qmlRegisterType<cwCavingRegion>(uri, 1, 0, "CavingRegion");
    qmlRegisterType<cwCave>(uri, 1, 0, "Cave");
    qmlRegisterType<cwSurveyChunk>();//uri, 1, 0, "cwSurveyChunk");
    qmlRegisterType<cwSurveyChunkView>(uri, 1, 0, "SurveyChunkView");
    qmlRegisterType<cwSurveyChunkGroupView>(uri, 1, 0, "SurveyChunkGroupView");
    qmlRegisterType<cwTrip>();
    qmlRegisterType<cwClinoValidator>(uri, 1, 0, "ClinoValidator");
    qmlRegisterType<cwStationValidator>(uri, 1, 0, "StationValidator");
    qmlRegisterType<cwCompassValidator>(uri, 1, 0, "CompassValidator");
    qmlRegisterType<cwDistanceValidator>(uri, 1, 0, "DistanceValidator");
    qmlRegisterType<cwSurveyNoteModel>(uri, 1, 0, "NoteModel");
    //qmlRegisterType<cwNoteItem>(uri, 1, 0, "NoteItem");
    qmlRegisterType<cwTreeView>(uri, 1, 0, "TreeView");
    qmlRegisterType<cwRegionTreeModel>(uri, 1, 0, "RegionTreeModel");
    qmlRegisterType<cwQMLWidget>(uri, 1, 0, "ProxyWidget");
    //    qmlRegisterType<QWidget>(uri, 1, 0, "QWidget");
    qmlRegisterType<cwUsedStationTaskManager>(uri, 1, 0, "UsedStationTaskManager");
    qmlRegisterType<cw3dRegionViewer>(uri, 1, 0, "RegionViewer");
    qmlRegisterType<QGLWidget>(uri, 1, 0, "QGLWidget");
    qmlRegisterType<cwLinePlotManager>(uri, 1, 0, "LinePlotManager");
    qmlRegisterType<cwGLLinePlot>(uri, 1, 0, "GLLinePlot");
    qmlRegisterType<cwFileDialogHelper>(uri, 1, 0, "FileDialogHelper");
    qmlRegisterType<cwProject>(uri, 1, 0, "Project");
    qmlRegisterType<cwNote>(uri, 1, 0, "Note");
    qmlRegisterType<cwBasePanZoomInteraction>(uri, 1, 0, "BasePanZoomInteraction");
    qmlRegisterType<cwBaseScrapInteraction>(uri, 1, 0, "BaseScrapInteraction");
    qmlRegisterType<cwCamera>(uri, 1, 0, "Camera");
    qmlRegisterType<cwImageItem>(uri, 1, 0, "ImageItem");
    qmlRegisterType<cwWheelArea>(uri, 1, 0, "WheelArea");
    qmlRegisterType<cwScrapView>(uri, 1, 0, "ScrapView");
    qmlRegisterType<cwTransformUpdater>(uri, 1, 0, "TransformUpdater");
    qmlRegisterType<cwNoteStationView>(uri, 1, 0, "NoteStationView");
    qmlRegisterType<cwScrapStationView>(uri, 1, 0, "ScrapStationView");
    qmlRegisterType<cwScrap>(uri, 1, 0, "Scrap");
    qmlRegisterType<cwBaseNoteStationInteraction>(uri, 1, 0, "BaseNoteStationInteraction");
    qmlRegisterType<cwNoteTranformation>(uri, 1, 0, "NoteTransform");
    qmlRegisterType<cwScrapItem>(uri, 1, 0, "ScrapItem");
    qmlRegisterType<cwLength>(uri, 1, 0, "Length");
    qmlRegisterType<cwInteraction>(uri, 1, 0, "Interaction");
//    qmlRegisterType<cwInteractionManager>(uri, 1, 0, "InteractionManager");
}
