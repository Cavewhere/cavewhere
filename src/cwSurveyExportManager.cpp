//Our includes
#include "cwSurveyExportManager.h"
#include "cwSurvexExporterTripTask.h"
#include "cwSurvexExporterCaveTask.h"
#include "cwSurvexExporterRegionTask.h"
#include "cwCompassExporterCaveTask.h"
#include "cwRegionTreeModel.h"
#include "cwDebug.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwGlobals.h"

//Qt includes
#include <QFileDialog>
#include <QMessageBox>
#include <QAction>
#include <QMenu>
#include <QItemSelectionModel>
#include <QDebug>
#include <QThread>

cwSurveyExportManager::cwSurveyExportManager(QObject *parent) :
    QObject(parent),
    Model(NULL),
    SelectionModel(NULL),
    ExportThread(new QThread(this))
{

}

/**
    Destructor
  */
cwSurveyExportManager::~cwSurveyExportManager() {
    ExportThread->exit();
    ExportThread->wait();
}

/**
  Sets the caving region tree model that this class uses to update it's menu option based
  on the current selection.
  */
void cwSurveyExportManager::setCavingRegionTreeModel(cwRegionTreeModel *model) {
    if(Model != model) {
        Model = model;
        connect(Model, SIGNAL(dataChanged(QModelIndex,QModelIndex)), SLOT(updateActions()));
    }
}

/**
  \brief Opens the survex exporter
  */
void cwSurveyExportManager::openExportSurvexTripFileDialog() {
    QString filename = QFileDialog::getSaveFileName(NULL, "Export trip to Survex", "", "Survex *.svx");
    filename = cwGlobals::addExtension(filename, "svx");
    exportSurvexTrip(filename);
}

/**
  \brief Asks the user to choose a file to export the currently selected file
  */
void cwSurveyExportManager::openExportSurvexCaveFileDialog() {
    QString filename = QFileDialog::getSaveFileName(NULL, "Export cave to Survex", "", "Survex *.svx");
    filename = cwGlobals::addExtension(filename, "svx");
    exportSurvexCave(filename);
}

/**
  cwSurveyEditorMainWindow
  \brief Open a file dialog for the user to save
  all the data to survex file
  */
void cwSurveyExportManager::openExportSurvexRegionFileDialog() {
    QString filename = QFileDialog::getSaveFileName(NULL, "Export region to Survex", "", "Survex *.svx");
    filename = cwGlobals::addExtension(filename, "svx");
    exportSurvexRegion(filename);
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
    QString filename = QFileDialog::getSaveFileName(NULL, "Export cave to Compass", "", "Compass *.dat");
    filename = cwGlobals::addExtension(filename, "dat");
    exportCaveToCompass(filename);
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
    if(filename.isEmpty()) { return; }

    cwSurvexExporterRegionTask* exportTask = new cwSurvexExporterRegionTask();
    exportTask->setOutputFile(filename);
    exportTask->setData(*(Model->cavingRegion()));
    connect(exportTask, SIGNAL(finished()), SLOT(exporterFinished()));
    connect(exportTask, SIGNAL(stopped()), SLOT(exporterFinished()));
    exportTask->setThread(ExportThread);
    exportTask->start();
}

/**
  \brief Exports the currently selected cave to a file
  */
void cwSurveyExportManager::exportSurvexCave(QString filename) {
    if(filename.isEmpty()) { return; }

    cwCave* cave = currentCave();

    if(cave != NULL) {
        cwSurvexExporterCaveTask* exportTask = new cwSurvexExporterCaveTask();
        exportTask->setOutputFile(filename);
        exportTask->setData(*cave);
        connect(exportTask, SIGNAL(finished()), SLOT(exporterFinished()));
        connect(exportTask, SIGNAL(stopped()), SLOT(exporterFinished()));
        exportTask->setThread(ExportThread);
        exportTask->start();
    }
}

/**
  \brief Exports the currently selected trip to filename
  */
void cwSurveyExportManager::exportSurvexTrip(QString filename) {
    if(filename.isEmpty()) { return; }
    cwTrip* trip = currentTrip();
    if(trip != NULL) {
        cwSurvexExporterTripTask* exportTask = new cwSurvexExporterTripTask();
        exportTask->setOutputFile(filename);
        exportTask->setData(*trip);
        connect(exportTask, SIGNAL(finished()), SLOT(exporterFinished()));
        connect(exportTask, SIGNAL(stopped()), SLOT(exporterFinished()));
        exportTask->setThread(ExportThread);
        exportTask->start();
    }
}

/**
  Exports the currently select cave to Compass
  */
void cwSurveyExportManager::exportCaveToCompass(QString filename) {
    Q_UNUSED(filename);
    if(filename.isEmpty()) { return; }

    cwCave* cave = currentCave();
    if(cave != NULL) {
        cwCompassExportCaveTask* exportTask = new cwCompassExportCaveTask();
        exportTask->setOutputFile(filename);
        exportTask->setData(*cave);
        connect(exportTask, SIGNAL(finished()), SLOT(exporterFinished()));
        connect(exportTask, SIGNAL(stopped()), SLOT(exporterFinished()));
        exportTask->setThread(ExportThread);
        exportTask->start();
    }
}

/**
  \brief Called when the export task has completed
  */
void cwSurveyExportManager::exporterFinished() {
    if(sender()) {
        sender()->deleteLater();
    }
}

/**
  Updates the current menu action with the current value of the
  selection model
  */
void cwSurveyExportManager::updateActions() {
    emit updateMenu();
}

/**
  Sets the region's selection model

  This allows the survey export manager to keep the export menu options update with the correct
  information
  */
void cwSurveyExportManager::setRegionSelectionModel(QItemSelectionModel *selectionModel) {

    //Make sure the selection model is setup correctly
    if(selectionModel->model() != Model) {
        qDebug() << "Can't set selection model because model isn't correct" << LOCATION;
        return;
    }

    if(selectionModel != SelectionModel) {
        if(SelectionModel != NULL) {
            disconnect(SelectionModel, NULL, this, NULL);
        }

        SelectionModel = selectionModel;

        if(SelectionModel != NULL) {
            connect(SelectionModel, SIGNAL(currentChanged(QModelIndex,QModelIndex)), SLOT(updateActions()));

            updateActions();
        }
    }
}

/**
Gets currentCaveName
*/
QString cwSurveyExportManager::currentCaveName() const {
    QModelIndex currentIndex = SelectionModel->currentIndex();
    if(Model->isTrip(currentIndex)) {
        currentIndex = currentIndex.parent();
    }

    if(Model->isCave(currentIndex)) {
        return Model->data(currentIndex, cwRegionTreeModel::NameRole).toString();
    }
    return QString();
}

/**
Gets currentTripName
*/
QString cwSurveyExportManager::currentTripName() const {
    QModelIndex currentIndex = SelectionModel->currentIndex();
    if(Model->isTrip(currentIndex)) {
        return Model->data(currentIndex, cwRegionTreeModel::NameRole).toString();
    }
    return QString();
}

/**
  Gets the current selected cave
  */
cwCave *cwSurveyExportManager::currentCave() const
{
    cwCave* cave = NULL;
    QModelIndex currentSelected = SelectionModel->currentIndex();
    if(Model->isTrip(currentSelected)) {
        cave = Model->trip(currentSelected)->parentCave();
    } else if(Model->isCave(currentSelected)) {
        cave = Model->cave(SelectionModel->currentIndex());
    }
    return cave;
}

/**
  Gets the current selected trip
  */
cwTrip* cwSurveyExportManager::currentTrip() const {
    return Model->trip(SelectionModel->currentIndex());
}
