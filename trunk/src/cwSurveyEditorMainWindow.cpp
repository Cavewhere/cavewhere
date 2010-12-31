//Our includes
#include "cwSurveyEditorMainWindow.h"
#include "cwSurveyChunk.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwSurvexImporter.h"
#include "cwSurvexExporter.h"
#include "cwSurveyTrip.h"
#include "cwSurveyChuckView.h"
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

//Qt includes
#include <QDeclarativeContext>
#include <QDeclarativeComponent>
#include <QFileDialog>
#include <QDebug>
#include <QSettings>
#include <QTreeView>



cwSurveyEditorMainWindow::cwSurveyEditorMainWindow(QWidget *parent) :
    QMainWindow(parent),
    SurvexExporter(NULL),
    Trip(new cwTrip(this)),
    NoteModel(new cwSurveyNoteModel(this))
{
    setupUi(this);
    DeclarativeView->setResizeMode(QDeclarativeView::SizeRootObjectToView);
    DeclarativeView->setRenderHint(QPainter::SmoothPixmapTransform, true);

    qmlRegisterType<cwStation>(); //"Cavewhere", 1, 0, "cwStation");
    qmlRegisterType<cwShot>(); //"Cavewhere", 1, 0, "cwShot");
    qmlRegisterType<cwSurveyChunk>();//"Cavewhere", 1, 0, "cwSurveyChunk");
    qmlRegisterType<cwSurveyChunkView>("Cavewhere", 1, 0, "SurveyChunkView");
    qmlRegisterType<cwSurveyChunkGroupView>("Cavewhere", 1, 0, "SurveyChunkGroupView");
    qmlRegisterType<cwTrip>();
    qmlRegisterType<cwClinoValidator>("Cavewhere", 1, 0, "ClinoValidator");
    qmlRegisterType<cwStationValidator>("Cavewhere", 1, 0, "StationValidator");
    qmlRegisterType<cwCompassValidator>("Cavewhere", 1, 0, "CompassValidator");
    qmlRegisterType<cwDistanceValidator>("Cavewhere", 1, 0, "DistanceValidator");
    qmlRegisterType<cwSurveyNoteModel>("Cavewhere", 1, 0, "NoteModel");
    qmlRegisterType<cwNoteItem>("Cavewhere", 1, 0, "NoteItem");
    qmlRegisterType<cwTreeView>("Cavewhere", 1, 0, "TreeView");
    qmlRegisterType<cwRegionTreeModel>("Cavewhere", 1, 0, "RegionTreeModel");
    qmlRegisterType<cwQMLWidget>("Cavewhere", 1, 0, "ProxyWidget");
    qmlRegisterType<QWidget>("Cavewhere", 1, 0, "QWidget");

    //qmlRegisterExtendedType<cwRegionTreeModel, QAbstractItemModel>("Cavewhere", 1, 0, "AbstractItemModel");

    connect(actionSurvexImport, SIGNAL(triggered()), SLOT(ImportSurvex()));
    connect(actionSurvexExport, SIGNAL(triggered()), SLOT(ExportSurvex()));
    connect(actionReloadQML, SIGNAL(triggered()), SLOT(ReloadQML()));

    //Initial chunk - for qml testing
    cwSurveyChunk* chunk = new cwSurveyChunk(Trip);
    chunk->AppendNewShot(); //Add the first shot

    QList<cwSurveyChunk*> chunks;
    chunks.append(chunk);

    Trip->setChucks(chunks);


    Region = new cwCavingRegion(this);
    RegionTreeModel = new cwRegionTreeModel(this);
    RegionTreeModel->setCavingRegion(Region);
    RegionTreeView = new QTreeView();
    RegionTreeView->setModel(RegionTreeModel);

    ReloadQML();

    //Setup interaction for selection
    QItemSelectionModel* regionSelectionModel = RegionTreeView->selectionModel();
    connect(regionSelectionModel, SIGNAL(selectionChanged(QItemSelection,QItemSelection)), SLOT(SetSurveyData(QItemSelection,QItemSelection)));

}

void cwSurveyEditorMainWindow::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        retranslateUi(this);
        break;
    default:
        break;
    }
}

/**
  \brief Opens the survex exporter
  */
void cwSurveyEditorMainWindow::ExportSurvex() {
    if(SurvexExporter == NULL) {
        SurvexExporter = new cwSurvexExporter(this);
    }

    SurvexExporter->setChunks(Trip);
    QFileDialog* dialog = new QFileDialog(NULL, "Export Survex", "", "Survex *.svx");
    dialog->setAcceptMode(QFileDialog::AcceptSave);
    dialog->open(SurvexExporter, SLOT(exportSurvex(QString)));

}

/**
  \brief Opens the suvrex import dialog
  */
void cwSurveyEditorMainWindow::ImportSurvex() {
    cwImportSurvexDialog* survexImportDialog = new cwImportSurvexDialog(Region, this);
    survexImportDialog->setAttribute(Qt::WA_DeleteOnClose, true);
    survexImportDialog->open();
}

/**
  \brief Updates the survey editor
  */
void cwSurveyEditorMainWindow::UpdateSurveyEditor() {
//    QList<cwSurveyChunk*> chunks = SurvexImporter->chunks();
    if(Trip == NULL) {
        Trip = new cwTrip(this);
    }
//    Trip->setChucks(chunks);

    ReloadQML();
}

void cwSurveyEditorMainWindow::ReloadQML() {
    QDeclarativeContext* context = DeclarativeView->rootContext();
    context->setParent(this);

    context->setContextProperty("surveyNoteModel", NoteModel);
    context->setContextProperty("surveyData", Trip);
    context->setContextProperty("regionTreeView", RegionTreeView);
    //context->setContextProperty("regionModel", RegionTreeModel);


    DeclarativeView->setSource(QUrl::fromLocalFile("qml/SurveyEditor.qml"));


//    RegionView  = context->findChild<cwTreeView*>("regionTree");
//    //qDebug() << "TreeVariant" << treeVariant << treeVariant.isNull();
//    //RegionView = qobject_cast<cwTreeView*>(treeVariant.value<QObject*>());
//    if(RegionView == NULL) {
//        qWarning() << "WTF mate: Can't find \"regionTree\" in qml/SurveyEditor.qml";
//    }

}

/**
  \brief Set's the survey data for the current editor
  */
void cwSurveyEditorMainWindow::SetSurveyData(QItemSelection selected, QItemSelection deselected) {

    QList<QModelIndex> selectedIndexes = selected.indexes();
    if(!selectedIndexes.isEmpty()) {
        QModelIndex firstSelected = selectedIndexes.first();
        QVariant objectVariant = firstSelected.data(cwRegionTreeModel::ObjectRole);
        cwTrip* trip = qobject_cast<cwTrip*>(objectVariant.value<QObject*>());
        cwCave* cave = qobject_cast<cwCave*>(objectVariant.value<QObject*>());
        if(trip != NULL) {
            QDeclarativeContext* context = DeclarativeView->rootContext();
            context->setContextProperty("surveyData", trip);

        } else if(cave != NULL) {
            //Do nothing for now
        }
    }
}

