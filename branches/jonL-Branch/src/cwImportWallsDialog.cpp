//Our includes
#include "cwImportWallsDialog.h"
#include "cwWallsImporterModel.h"
#include "cwWallsImporter.h"
#include "cwGlobalIcons.h"
#include "cwWallsBlockData.h"
#include "cwWallsGlobalData.h"
#include "cwCavingRegion.h"
#include "cwTaskProgressDialog.h"
#include "cwStringListErrorModel.h"
#include "cwGlobalUndoStack.h"

//Qt includes
#include <QFileSystemModel>
#include <QSettings>
#include <QFileDialog>
#include <QItemSelectionModel>
#include <QPixmapCache>
#include <QMessageBox>
#include <QDebug>
#include <QThread>


const QString cwImportWallsDialog::ImportWallsKey = "LastImportWallsFile";

cwImportWallsDialog::cwImportWallsDialog(cwCavingRegion* region, QWidget *parent) :
    QDialog(parent),
    Region(region),
    Model(new cwWallsImporterModel(this)),
    Importer(new cwWallsImporter()),
    WallsSelectionModel(new QItemSelectionModel(Model, this)),
    ImportThread(new QThread(this))
{
    setupUi(this);
    setupTypeComboBox();

    //Move the importer to another thread
    Importer->setThread(ImportThread);

    //Connect the importer up
    connect(Importer, SIGNAL(finished()), SLOT(importerFinishedRunning()));
    connect(Importer, SIGNAL(stopped()), SLOT(importerCanceled()));

    connect(WallsSelectionModel, SIGNAL(selectionChanged(QItemSelection,QItemSelection)), SLOT(updateCurrentItem(QItemSelection,QItemSelection)));
    connect(ImportButton, SIGNAL(clicked()), SLOT(import()));

    WallsTreeView->setModel(Model);
    WallsTreeView->setSelectionModel(WallsSelectionModel);
    WallsTreeView->setHeaderHidden(true);
    WallsTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    splitter->setStretchFactor(1, 4);

    WallsErrorListView->setWordWrap(true);
    //WallsErrorListView->setUniformItemSizes(true);

    setWindowTitle("Walls Importer");
}

cwImportWallsDialog::~cwImportWallsDialog() {
    Importer->stop();

    ImportThread->quit();
    ImportThread->wait();

    delete Importer;
}

/**
  \brief Start to the dialog to import
  */
void cwImportWallsDialog::open() {
    if(Region == NULL) {
        QMessageBox box(QMessageBox::Critical, "Broke Sauce!", "Oops, the programer has made a mistake and Cavewhere can't open Walls Import Dialog.");
        box.exec();
        return;
    }

    QSettings settings;
    QString lastFile = settings.value(ImportWallsKey).toString();

    QFileDialog* dialog = new QFileDialog(NULL, "Import Walls files", lastFile, "Walls (*.wpj *.srv)");
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->open(this, SLOT(setWallsFile(QString)));
}

/**
  \brief The Walls file that'll be opened
  */
void cwImportWallsDialog::setWallsFile(QString filename) {
    QSettings settings;
    settings.setValue(ImportWallsKey, filename);

    //The root filename
    FullFilename = filename;

    //Show a progress dialog
    cwTaskProgressDialog* progressDialog = new cwTaskProgressDialog(this);
    progressDialog->setTask(Importer);
    progressDialog->setAttribute(Qt::WA_DeleteOnClose, true);
    progressDialog->show();

    //Run the importer on another thread
    QMetaObject::invokeMethod(Importer, "setWallsFile", Q_ARG(QString, filename));
    Importer->start();
}


void cwImportWallsDialog::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        retranslateUi(this);
        break;
    default:
        break;
    }
}

/**
  \brief When the dialog is resized
  */
void cwImportWallsDialog::resizeEvent(QResizeEvent* event) {
    QDialog::resizeEvent(event);

    QString cuttOffText =  FileLabel->fontMetrics().elidedText(FullFilename, Qt::ElideMiddle, FileLabel->width());
    FileLabel->setText(cuttOffText);
}



/**
  \brief Sets up the combo box for this dialog
  */
void cwImportWallsDialog::setupTypeComboBox() {
    QPixmap dontImportIcon;
    if(!QPixmapCache::find(cwGlobalIcons::NoImport, &dontImportIcon)) {
        dontImportIcon = QPixmap(cwGlobalIcons::NoImportFilename);
        cwGlobalIcons::NoImport = QPixmapCache::insert(dontImportIcon);
    }

    QPixmap caveIcon;
    if(!QPixmapCache::find(cwGlobalIcons::Cave, &caveIcon)) {
        caveIcon = QPixmap(cwGlobalIcons::CaveFilename);
        cwGlobalIcons::Cave = QPixmapCache::insert(caveIcon);
    }

    QPixmap tripIcon;
    if(!QPixmapCache::find(cwGlobalIcons::Trip, &tripIcon)) {
        tripIcon = QPixmap(cwGlobalIcons::TripFilename);
        cwGlobalIcons::Trip = QPixmapCache::insert(tripIcon);
    }
}

/**
  \brief This will go through the cwWallsGlobalData and see if it's possible to import it

  The main purpose of this dialog is to check for name collision for stations in a cave
  */
void cwImportWallsDialog::updateImportErrors() {

}

/**
  \brief Updates the import warning label next to the import button
  */
void cwImportWallsDialog::updateImportWarningLabel() {
    QStringList errors = Importer->errors();
    int numberWarnings = 0;
    int numberErrors = 0;
    foreach(QString error, errors) {
        if(error.contains("Error:")) {
            numberErrors++;
        } else if(error.contains("Warning:")) {
            numberWarnings++;
        }
    }

    QString message;
    if(numberErrors > 0) {
        message.append(QString("<b>Errors:%1</b> ").arg(numberErrors));
    }
    if(numberWarnings > 0) {
        message.append(QString("Warnings:%2").arg(numberWarnings));
    }
    WarningLabel->setText(message);
}


/**
  \brief Updates this view with the current items that are selected
  */
void cwImportWallsDialog::updateCurrentItem(QItemSelection selected, QItemSelection /*deselected*/) {
    QModelIndexList selectedIndexes = selected.indexes();
    if(selectedIndexes.size() == 1) {
        //Only one item selected
        QModelIndex index = selectedIndexes.first();
        cwWallsBlockData* block = Model->toBlockData(index);
        if(block != NULL) {

            //Set the text for the combo box
            QString name = block->name();
            QString noImportStrting = QString("%1 %2").arg(cwWallsBlockData::importTypeToString(cwWallsBlockData::NoImport)).arg(name);
            QString caveString  = QString("%1 is a %2").arg(name).arg(cwWallsBlockData::importTypeToString(cwWallsBlockData::Cave));
            QString tripString = QString("%1 is a %2").arg(name).arg(cwWallsBlockData::importTypeToString(cwWallsBlockData::Trip));

            return;
        }
    }
}

/**
  \brief Sets the type for the currently select item(s)

  If multiple items are selected then
  */
void cwImportWallsDialog::setType(int index) {

    QModelIndexList selectedIndexes = WallsSelectionModel->selectedIndexes();

    foreach(QModelIndex currentIndex, selectedIndexes) {
        cwWallsBlockData* block = Model->toBlockData(currentIndex);

        if(block != NULL) {
            cwWallsBlockData::ImportType importType = (cwWallsBlockData::ImportType)typeItemToImportType((TypeItem)index);
            block->setImportType(importType);
        }
    }
}


/**
  \brief Converts a typeItem to a cwWallsBlockData::ImportType
  */
int cwImportWallsDialog::typeItemToImportType(TypeItem typeItem) const {
    switch(typeItem) {
    case NoImportItem:
        return cwWallsBlockData::NoImport;
    case CaveItem:
        return cwWallsBlockData::Cave;
    case TripItem:
        return cwWallsBlockData::Trip;
    default:
        return -1;
    }
}

    /**
      \brief Converts a cwWallsBlockData::ImportType to a typeItem
      */
cwImportWallsDialog::TypeItem cwImportWallsDialog::importTypeToTypeItem(int type) const {
    switch(type) {
    case cwWallsBlockData::NoImport:
        return NoImportItem;
    case cwWallsBlockData::Cave:
        return CaveItem;
    case cwWallsBlockData::Trip:
        return TripItem;
    }
    return (cwImportWallsDialog::TypeItem)-1;
}

/**
  \brief Tries to import
  */
void cwImportWallsDialog::import() {
    cwWallsGlobalData* globalData = Importer->data();

    if(!globalData->caves().isEmpty()) {
        beginUndoMacro("Import Walls");
        Region->addCaves(globalData->caves());
        endUndoMacro();
    }
    accept();
}

/**
  \brief Called when the importer has finished

  All the data has been parsed out of the importer
  */
void cwImportWallsDialog::importerFinishedRunning() {
    //Move the importer back to this thread
    Importer->setThread(thread(), Qt::BlockingQueuedConnection);

    //Get the importer's data
    Model->setWallsData(Importer->data());

    //Cutoff to long text
    QString cutOffText = FileLabel->fontMetrics().elidedText(FullFilename, Qt::ElideMiddle, FileLabel->width());
    FileLabel->setText(cutOffText);

    //Load the error list view
    cwStringListErrorModel* parsingErrorsModel = new cwStringListErrorModel(this);
    parsingErrorsModel->setStringList(Importer->errors());
    WallsErrorListView->setModel(parsingErrorsModel);

    //Update the error / warning label at the bottom
    updateImportWarningLabel();

    //shut down the thread
    ImportThread->quit();

    show();
}

/**
  \brief Called if the importer has been canceled by the user
  */
void cwImportWallsDialog::importerCanceled() {
    ImportThread->quit();
    close();
}
