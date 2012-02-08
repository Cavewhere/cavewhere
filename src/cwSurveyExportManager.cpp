//Our includes
#include "cwSurveyExportManager.h"
#include "cwRegionTreeModel.h"

//Qt includes
#include <QFileDialog>
#include <QMessageBox>

cwSurveyExportManager::cwSurveyExportManager(QObject *parent) :
    QObject(parent),
    SurvexMenu(new QMenu()),
    CompassMenu(new QMenu()),
    ExportSurvexTripAction(new QAction(this)),
    ExportSurvexCaveAction(new QAction(this)),
    ExportSurvexRegionAction(new QAction("Region (all caves)", this)),
    ExportCompassTripAction(new QAction(this)),
    ExportCompassCaveAction(new QAction(this)),
    ExportCompassRegionAction(new QAction("Makefile (all caves)", this))
{
    SurvexMenu->addAction(ExportSurvexTripAction);
    SurvexMenu->addAction(ExportSurvexCaveAction);
    SurvexMenu->addAction(ExportSurvexRegionAction);

    CompassMenu->addAction(ExportCompassTripAction);
    CompassMenu->addAction(ExportCompassCaveAction);
    CompassMenu->addAction(ExportCompassRegionAction);

    //Connect everything up
    connect(ExportSurvexTripAction, SIGNAL(triggered()), SLOT(openExportSurvexTripFileDialog()));
    connect(ExportSurvexCaveAction, SIGNAL(triggered()), SLOT(openExportSurvexCaveFileDialog()));
    connect(ExportSurvexRegionAction, SIGNAL(triggered()), SLOT(openExportSurvexRegionFileDialog()));
    connect(ExportCompassTripAction, SIGNAL(triggered()), SLOT(openExportCompassTripFileDialog()));
    connect(ExportCompassCaveAction, SIGNAL(triggered()), SLOT(openExportCompassCaveFileDialog()));
    connect(ExportCompassRegionAction, SIGNAL(triggered()), SLOT(openExportCompassRegionFileDialog()));

}

/**
  Gets all the menu's that this survey export manager has

  Currently it return the survex and compass menu
  */
QList<QMenu*> cwSurveyExportManager::menus() const {
    QList<QMenu*> rootMenus;
    rootMenus.append(SurvexMenu);
    rootMenus.append(CompassMenu);
    return rootMenus;
}

/**
  Sets the caving region tree model that this class uses to update it's menu option based
  on the current selection.
  */
void cwSurveyExportManager::setCavingRegionTreeModel(cwRegionTreeModel *model) {

}

/**
  \brief Opens the survex exporter
  */
void cwSurveyExportManager::openExportSurvexTripFileDialog() {
    QFileDialog* dialog = new QFileDialog(NULL, "Export Survex", "", "Survex *.svx");
    dialog->setAcceptMode(QFileDialog::AcceptSave);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->open(this, SLOT(exportSurvexTrip(QString)));
}

/**
  \brief Asks the user to choose a file to export the currently selected file
  */
void cwSurveyExportManager::openExportSurvexCaveFileDialog() {
    QFileDialog* dialog = new QFileDialog(NULL, "Export to Survex", "", "Survex *.svx");
    dialog->setAcceptMode(QFileDialog::AcceptSave);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->open(this, SLOT(exportSurvexCave(QString)));
}

/**
  cwSurveyEditorMainWindow
  \brief Open a file dialog for the user to save
  all the data to survex file
  */
void cwSurveyExportManager::openExportSurvexRegionFileDialog() {
    QFileDialog* dialog = new QFileDialog(NULL, "Export to Survex", "", "Survex *.svx");
    dialog->setAcceptMode(QFileDialog::AcceptSave);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->open(this, SLOT(exportSurvexRegion(QString)));
}

void cwSurveyExportManager::openExportCompassTripFileDialog()
{
    QMessageBox* messageBox = new QMessageBox();
    messageBox->setAttribute(Qt::WA_DeleteOnClose, true);
    messageBox->setText("Currently exporting a single trip to compass <b>isn't supported</b>");
    messageBox->show();
}

/**
  Opens the compass file export dialog
  */
void cwSurveyExportManager::openExportCompassCaveFileDialog() {
    QFileDialog* dialog = new QFileDialog(NULL, "Export selected cave to Compass", "", "Compass *.dat");
    dialog->setAcceptMode(QFileDialog::AcceptSave);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->open(this, SLOT(exportCaveToCompass(QString)));
}

void cwSurveyExportManager::openExportCompassRegionFileDialog()
{
    QMessageBox* messageBox = new QMessageBox();
    messageBox->setAttribute(Qt::WA_DeleteOnClose, true);
    messageBox->setText("Currently exporting a compass Makefile <b>isn't supported</b>");
    messageBox->show();
}

/**
  \brief Exports the survex region to filename
  */
void cwSurveyExportManager::exportSurvexRegion(QString filename) {
//    cwSurvexExporterRegionTask* exportTask = new cwSurvexExporterRegionTask();
//    exportTask->setOutputFile(filename);
//    exportTask->setData(*(Data->region()));
//    connect(exportTask, SIGNAL(finished()), SLOT(exporterFinished()));
//    connect(exportTask, SIGNAL(stopped()), SLOT(exporterFinished()));
//    exportTask->setThread(ExportThread);
//    exportTask->start();
}

/**
  \brief Exports the currently selected cave to a file
  */
void cwSurveyExportManager::exportSurvexCave(QString filename) {
//    if(filename.isEmpty()) { return; }
//    cwCave* cave = currentSelectedCave();
//    if(cave != NULL) {
//        cwSurvexExporterCaveTask* exportTask = new cwSurvexExporterCaveTask();
//        exportTask->setOutputFile(filename);
//        exportTask->setData(*cave);
//        connect(exportTask, SIGNAL(finished()), SLOT(exporterFinished()));
//        connect(exportTask, SIGNAL(stopped()), SLOT(exporterFinished()));
//        exportTask->setThread(ExportThread);
//        exportTask->start();
//    }
}

/**
  \brief Exports the currently selected trip to filename
  */
void cwSurveyExportManager::exportSurvexTrip(QString filename) {
//    if(filename.isEmpty()) { return; }

//    cwTrip* trip = NULL //currentSelectedTrip();
//    if(trip != NULL) {
//        cwSurvexExporterTripTask* exportTask = new cwSurvexExporterTripTask();
//        exportTask->setOutputFile(filename);
//        exportTask->setData(*trip);
//        connect(exportTask, SIGNAL(finished()), SLOT(exporterFinished()));
//        connect(exportTask, SIGNAL(stopped()), SLOT(exporterFinished()));
//        exportTask->setThread(ExportThread);
//        exportTask->start();
//    }
}

/**
  Exports the currently select cave to Compass
  */
void cwSurveyExportManager::exportCaveToCompass(QString filename) {
//        if(filename.isEmpty()) { return; }
//        if(!Data->region()->hasCaves()) { return; }

//        cwCave* cave = Data->region()->cave(0);
//        if(cave != NULL) {
//            cwCompassExportCaveTask* exportTask = new cwCompassExportCaveTask();
//            exportTask->setOutputFile(filename);
//            exportTask->setData(*cave);
//            connect(exportTask, SIGNAL(finished()), SLOT(exporterFinished()));
//            connect(exportTask, SIGNAL(stopped()), SLOT(exporterFinished()));
//            exportTask->setThread(ExportThread);
//            exportTask->start();
//        }
}

/**
  \brief Called when the export task has completed
  */
void cwSurveyExportManager::exporterFinished() {
    if(sender()) {
        sender()->deleteLater();
    }
}
