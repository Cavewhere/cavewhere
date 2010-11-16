//Our includes
#include "cwSurveyEditorMainWindow.h"
#include "cwSurveyChunk.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwSurveyImporter.h"
#include "cwSurvexExporter.h"
#include "cwSurveyChunkGroup.h"
#include "cwSurveyChuckView.h"
#include "cwSurveyChunkGroupView.h"
#include "cwClinoValidator.h"
#include "cwStationValidator.h"
#include "cwCompassValidator.h"
#include "cwDistanceValidator.h"

//Qt includes
#include <QDeclarativeContext>
#include <QDeclarativeComponent>
#include <QFileDialog>
#include <QDebug>

cwSurveyEditorMainWindow::cwSurveyEditorMainWindow(QWidget *parent) :
    QMainWindow(parent),
    SurvexImporter(NULL),
    SurvexExporter(NULL),
    ChunkGroup(new cwSurveyChunkGroup(this))
{
    setupUi(this);
    DeclarativeView->setResizeMode(QDeclarativeView::SizeRootObjectToView);

    qmlRegisterType<cwStation>(); //"Cavewhere", 1, 0, "cwStation");
    qmlRegisterType<cwShot>(); //"Cavewhere", 1, 0, "cwShot");
    qmlRegisterType<cwSurveyChunk>();//"Cavewhere", 1, 0, "cwSurveyChunk");
    qmlRegisterType<cwSurveyChunkView>("Cavewhere", 1, 0, "SurveyChunkView");
    qmlRegisterType<cwSurveyChunkGroupView>("Cavewhere", 1, 0, "SurveyChunkGroupView");
    qmlRegisterType<cwSurveyChunkGroup>();
    qmlRegisterType<cwClinoValidator>("Cavewhere", 1, 0, "ClinoValidator");
    qmlRegisterType<cwStationValidator>("Cavewhere", 1, 0, "StationValidator");
    qmlRegisterType<cwCompassValidator>("Cavewhere", 1, 0, "CompassValidator");
    qmlRegisterType<cwDistanceValidator>("Cavewhere", 1, 0, "DistanceValidator");

    connect(actionSurvexImport, SIGNAL(triggered()), SLOT(ImportSurvex()));
    connect(actionSurvexExport, SIGNAL(triggered()), SLOT(ExportSurvex()));
    connect(actionReloadQML, SIGNAL(triggered()), SLOT(ReloadQML()));

    //Initial chunk
    cwSurveyChunk* chunk = new cwSurveyChunk(ChunkGroup);
    chunk->AppendNewShot(); //Add the first shot

    QList<cwSurveyChunk*> chunks;
    chunks.append(chunk);

    ChunkGroup->setChucks(chunks);
    ReloadQML();
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

    SurvexExporter->setChunks(ChunkGroup);
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
    if(ChunkGroup == NULL) {
        ChunkGroup = new cwSurveyChunkGroup(this);
    }
    ChunkGroup->setChucks(chunks);

    ReloadQML();
}

void cwSurveyEditorMainWindow::ReloadQML() {
    QDeclarativeContext* context = DeclarativeView->rootContext();

//    QList<QObject*> objects;
//    foreach(cwSurveyChunk* chunk, ChunkGroup->chunks()) {
//        qDebug() << "Chunk" << chunk;
//        objects.append(chunk);
//    }

    context->setContextProperty("surveyData", ChunkGroup);
    //context->setContextProperty("testChunk", ChunkGroup->chunk(0)); //set the first chunk

    DeclarativeView->setSource(QUrl::fromLocalFile("qml/SurveyEditor.qml"));
}

