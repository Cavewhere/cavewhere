/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
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
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwRegionTreeModel.h"
#include "cwLinePlotManager.h"
#include "cwUsedStationTaskManager.h"
#include "cwGlobalUndoStack.h"
#include "cwGLLinePlot.h"
#include "cw3dRegionViewer.h"
#include "cwProject.h"
#include "cwFileDialogHelper.h"
#include "cwImageProvider.h"
#include "cwXMLProjectLoadSaveTask.h"
#include "cwBasePanZoomInteraction.h"
#include "cwBaseScrapInteraction.h"
#include "cwCamera.h"
#include "cwImageItem.h"
#include "cwScrapView.h"
#include "cwScrapItem.h"
#include "cwTransformUpdater.h"
#include "cwBaseNoteStationInteraction.h"
#include "cwScrap.h"
#include "cwScrapStationView.h"
#include "cwNoteTranformation.h"
#include "cwScrapItem.h"
#include "cwLength.h"
#include "cwInteraction.h"
#include "cwNorthArrowItem.h"
#include "cwPositioner3D.h"
#include "cwAbstract2PointItem.h"
#include "cwScaleLengthItem.h"
#include "cwImageProperties.h"
#include "cwUnits.h"
#include "cwGLScraps.h"
#include "cwScrapManager.h"
#include "cwTeam.h"
#include "cwTripCalibration.h"
#include "cwSurveyChunkTrimmer.h"
#include "cwItemSelectionModel.h"
#include "cwSurveyExportManager.h"
#include "cwSurveyImportManager.h"
#include "cwTripLengthTask.h"
#include "cwLabel3dView.h"
#include "cwLinePlotLabelView.h"
#include "cwAbstractPointManager.h"
#include "cwScrapOutlinePointView.h"
#include "cwSelectionManager.h"
#include "cwUnitValue.h"
#include "cwImageResolution.h"
#include "cwExportRegionViewerToImageTask.h"
#include "cwOrthogonalProjection.h"
#include "cwPerspectiveProjection.h"
#include "cwMatrix4x4Animation.h"
#include "cwImageValidator.h"
#include "cwQMLReload.h"
#include "cwQuaternionRotation3d.h"
#include "cwCompassItem.h"
#include "cwShaderDebugger.h"
#include "cwLicenseAgreement.h"

//Qt registeration
#include <QQuickView>

cwQMLRegister::cwQMLRegister()
{
}

void cwQMLRegister::registerQML()
{
////    const char* uri = "Cavewhere";
    qmlRegisterType<cwCavingRegion>("Cavewhere", 1, 0, "CavingRegion");
    qmlRegisterType<cwCave>("Cavewhere", 1, 0, "Cave");
    qmlRegisterType<cwSurveyChunk>("Cavewhere", 1, 0, "SurveyChunk");
    qmlRegisterType<cwSurveyChunkView>("Cavewhere", 1, 0, "SurveyChunkView");
    qmlRegisterType<cwSurveyChunkGroupView>("Cavewhere", 1, 0, "SurveyChunkGroupView");
    qmlRegisterType<cwTrip>("Cavewhere", 1, 0, "Trip");
    qmlRegisterType<cwValidator>("Cavewhere", 1, 0, "Validator");
    qmlRegisterType<cwClinoValidator>("Cavewhere", 1, 0, "ClinoValidator");
    qmlRegisterType<cwStationValidator>("Cavewhere", 1, 0, "StationValidator");
    qmlRegisterType<cwCompassValidator>("Cavewhere", 1, 0, "CompassValidator");
    qmlRegisterType<cwDistanceValidator>("Cavewhere", 1, 0, "DistanceValidator");
    qmlRegisterType<cwSurveyNoteModel>("Cavewhere", 1, 0, "NoteModel");
    qmlRegisterType<cwRegionTreeModel>("Cavewhere", 1, 0, "RegionTreeModel");
    qmlRegisterType<cwUsedStationTaskManager>("Cavewhere", 1, 0, "UsedStationTaskManager");
    qmlRegisterType<cw3dRegionViewer>("Cavewhere", 1, 0, "RegionViewer");
    qmlRegisterType<cwLinePlotManager>("Cavewhere", 1, 0, "LinePlotManager");
    qmlRegisterType<cwGLLinePlot>("Cavewhere", 1, 0, "GLLinePlot");
    qmlRegisterType<cwFileDialogHelper>("Cavewhere", 1, 0, "FileDialogHelper");
    qmlRegisterType<cwProject>("Cavewhere", 1, 0, "Project");
    qmlRegisterType<cwNote>("Cavewhere", 1, 0, "Note");
    qmlRegisterType<cwBasePanZoomInteraction>("Cavewhere", 1, 0, "BasePanZoomInteraction");
    qmlRegisterType<cwBaseScrapInteraction>("Cavewhere", 1, 0, "BaseScrapInteraction");
    qmlRegisterType<cwCamera>("Cavewhere", 1, 0, "Camera");
    qmlRegisterType<cwImageItem>("Cavewhere", 1, 0, "ImageItem");
    qmlRegisterType<cwScrapView>("Cavewhere", 1, 0, "ScrapView");
    qmlRegisterType<cwTransformUpdater>("Cavewhere", 1, 0, "TransformUpdater");
    qmlRegisterType<cwScrapStationView>("Cavewhere", 1, 0, "ScrapStationView");
    qmlRegisterType<cwScrap>("Cavewhere", 1, 0, "Scrap");
    qmlRegisterType<cwBaseNoteStationInteraction>("Cavewhere", 1, 0, "BaseNoteStationInteraction");
    qmlRegisterType<cwNoteTranformation>("Cavewhere", 1, 0, "NoteTransform");
    qmlRegisterType<cwScrapItem>("Cavewhere", 1, 0, "ScrapItem");
    qmlRegisterType<cwLength>("Cavewhere", 1, 0, "Length");
    qmlRegisterType<cwInteraction>("Cavewhere", 1, 0, "Interaction");
    qmlRegisterType<cwNorthArrowItem>("Cavewhere", 1, 0, "NorthArrowItem");
    qmlRegisterType<cwPositioner3D>("Cavewhere", 1, 0, "Positioner3D");
    qmlRegisterType<cwAbstract2PointItem>();
    qmlRegisterType<cwScaleLengthItem>("Cavewhere", 1 ,0, "ScaleLengthItem");
    qmlRegisterType<cwImageProperties>("Cavewhere", 1, 0, "ImageProperties");
    qmlRegisterType<cwUnits>("Cavewhere", 1, 0, "Units");
    qmlRegisterType<cwGLScraps>("Cavewhere", 1, 0, "GLScraps");
    qmlRegisterType<cwScrapManager>("Cavewhere", 1, 0, "ScrapManager");
    qmlRegisterType<cwTeam>("Cavewhere", 1, 0, "Team");
    qmlRegisterType<cwTripCalibration>("Cavewhere", 1, 0, "Calibration");
    qmlRegisterType<cwSurveyChunkTrimmer>("Cavewhere", 1, 0, "SurveyChunkTrimmer");
    qmlRegisterType<cwItemSelectionModel>("Cavewhere", 1, 0, "ItemSelectionModel");
    qmlRegisterType<cwSurveyExportManager>("Cavewhere", 1, 0, "SurveyExportManager");
    qmlRegisterType<cwSurveyImportManager>("Cavawhere", 1, 0, "SurveyImportManager");
    qmlRegisterType<cwTripLengthTask>("Cavewhere", 1, 0, "TripLengthTask");
    qmlRegisterType<cwLabel3dView>("Cavewhere", 1, 0, "Label3dView");
    qmlRegisterType<cwLinePlotLabelView>("Cavewhere", 1, 0, "LinePlotLabelView");
    qmlRegisterType<cwScrapItem>("Cavewhere", 1, 0, "ScrapItem");
    qmlRegisterType<cwAbstractPointManager>();
    qmlRegisterType<cwScrapOutlinePointView>("Cavewhere", 1, 0, "ScrapControlPointView");
    qmlRegisterType<cwSelectionManager>("Cavewhere", 1, 0, "SelectionManager");
    qmlRegisterType<cwUnitValue>();
    qmlRegisterType<cwImageResolution>("Cavewhere", 1, 0, "ImageResolution");
    qmlRegisterType<cwExportRegionViewerToImageTask>("Cavewhere", 1, 0, "ExportRegioonViewerToImageTask");
    qmlRegisterType<cwOrthogonalProjection>("Cavewhere", 1, 0, "OrthogonalProjection");
    qmlRegisterType<cwPerspectiveProjection>("Cavewhere", 1, 0, "PerspectiveProjection");
    qmlRegisterType<cwMatrix4x4Animation>("Cavewhere", 1, 0, "Matrix4x4Animation");
    qmlRegisterType<cwImageValidator>("Cavewhere", 1, 0, "ImageValidator");
    qmlRegisterType<cwQMLReload>("Cavewhere", 1, 0, "QMLReload");
    qmlRegisterType<cwQuaternionRotation3d>("Cavewhere", 1, 0, "QuaternionRotation3d");
    qmlRegisterType<cwCompassItem>("Cavewhere", 1, 0, "CompassItem");
    qmlRegisterType<cwShaderDebugger>("Cavewhere", 1, 0, "ShaderDebugger");
    qmlRegisterType<cwLicenseAgreement>("Cavewhere", 1, 0, "LicenseAgreement");

    qmlRegisterType<QQuickView>("Cavewhere", 1, 0, "QQuickView");
}
