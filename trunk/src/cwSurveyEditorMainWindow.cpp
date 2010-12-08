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

//Qt includes
#include <QDeclarativeContext>
#include <QDeclarativeComponent>
#include <QFileDialog>
#include <QDebug>

cwSurveyEditorMainWindow::cwSurveyEditorMainWindow(QWidget *parent) :
    QMainWindow(parent),
    SurvexImporter(NULL),
    SurvexExporter(NULL),
    Trip(new cwSurveyTrip(this)),
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
    qmlRegisterType<cwSurveyTrip>();
    qmlRegisterType<cwClinoValidator>("Cavewhere", 1, 0, "ClinoValidator");
    qmlRegisterType<cwStationValidator>("Cavewhere", 1, 0, "StationValidator");
    qmlRegisterType<cwCompassValidator>("Cavewhere", 1, 0, "CompassValidator");
    qmlRegisterType<cwDistanceValidator>("Cavewhere", 1, 0, "DistanceValidator");
    qmlRegisterType<cwSurveyNoteModel>("Cavewhere", 1, 0, "NoteModel");
    qmlRegisterType<cwNoteItem>("Cavewhere", 1, 0, "NoteItem");

    connect(actionSurvexImport, SIGNAL(triggered()), SLOT(ImportSurvex()));
    connect(actionSurvexExport, SIGNAL(triggered()), SLOT(ExportSurvex()));
    connect(actionReloadQML, SIGNAL(triggered()), SLOT(ReloadQML()));

    //Initial chunk - for qml testing
    cwSurveyChunk* chunk = new cwSurveyChunk(Trip);
    chunk->AppendNewShot(); //Add the first shot

    QList<cwSurveyChunk*> chunks;
    chunks.append(chunk);

    Trip->setChucks(chunks);
    ReloadQML();

    //Trips
    cwSurveyTrip* trip1 = new cwSurveyTrip();
    cwSurveyTrip* trip2 = new cwSurveyTrip();
    cwSurveyTrip* trip3 = new cwSurveyTrip();
    cwSurveyTrip* trip4 = new cwSurveyTrip();
    cwSurveyTrip* trip5 = new cwSurveyTrip();
    cwSurveyTrip* trip6 = new cwSurveyTrip();

    cwCave* cave1 = new cwCave();
    cwCave* cave2 = new cwCave();
    cwCave* cave3 = new cwCave();

    trip1->setName("trip 1");
    trip2->setName("trip 2");
    trip3->setName("trip 3");
    trip4->setName("Sauce trip 1");
    trip5->setName("Sauce trip 2");
    trip6->setName("Me trip 1");

    cave1->setName("Cave 1");
    cave2->setName("Cave 2");
    cave3->setName("Cave 3");

    cave1->addTrip(trip1);
    cave1->addTrip(trip2);
    cave1->addTrip(trip3);

    cave2->addTrip(trip4);
    cave2->addTrip(trip5);

    cave3->addTrip(trip6);

    cwCavingRegion* region = new cwCavingRegion(this);
    region->addCave(cave1);
    region->addCave(cave2);
    region->addCave(cave3);

    cwRegionTreeModel* regionTree = new cwRegionTreeModel(this);
    regionTree->setCavingRegion(region);

    DataTreeView->setModel(regionTree);
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
    if(SurvexImporter == NULL) {
        SurvexImporter = new cwSurveyImporter(this);
        connect(SurvexImporter, SIGNAL(finishedImporting()), SLOT(UpdateSurveyEditor()));
    }

    QFileDialog* dialog = new QFileDialog(NULL, "Import Survex", SurvexImporter->lastImport(), "Survex *.svx");
    dialog->open(SurvexImporter, SLOT(importSurvex(QString)));
}

/**
  \brief Updates the survey editor
  */
void cwSurveyEditorMainWindow::UpdateSurveyEditor() {
    QList<cwSurveyChunk*> chunks = SurvexImporter->chunks();
    if(Trip == NULL) {
        Trip = new cwSurveyTrip(this);
    }
    Trip->setChucks(chunks);

    ReloadQML();
}

void cwSurveyEditorMainWindow::ReloadQML() {
    QDeclarativeContext* context = DeclarativeView->rootContext();

//    QList<QObject*> objects;
//    foreach(cwSurveyChunk* chunk, ChunkGroup->chunks()) {
//        qDebug() << "Chunk" << chunk;
//        objects.append(chunk);
//    }

    context->setContextProperty("surveyNoteModel", NoteModel);
    context->setContextProperty("surveyData", Trip);
    //context->setContextProperty("testChunk", ChunkGroup->chunk(0)); //set the first chunk

    DeclarativeView->setSource(QUrl::fromLocalFile("qml/SurveyEditor.qml"));

}

