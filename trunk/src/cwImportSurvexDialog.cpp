//Our includes
#include "cwImportSurvexDialog.h"
#include "cwSurvexImporterModel.h"
#include "cwSurvexImporter.h"
#include "cwGlobalIcons.h"
#include "cwSurvexBlockData.h"
#include "cwSurvexGlobalData.h"
#include "cwCavingRegion.h"
#include "cwTaskProgressDialog.h"

//Qt includes
#include <QFileSystemModel>
#include <QSettings>
#include <QFileDialog>
#include <QItemSelectionModel>
#include <QPixmapCache>
#include <QMessageBox>
#include <QDebug>
#include <QThread>


const QString cwImportSurvexDialog::ImportSurvexKey = "LastImportSurvexFile";

cwImportSurvexDialog::cwImportSurvexDialog(cwCavingRegion* region, QWidget *parent) :
    QDialog(parent),
    Region(region),
    Model(new cwSurvexImporterModel(this)),
    Importer(new cwSurvexImporter()),
    SurvexSelectionModel(new QItemSelectionModel(Model, this)),
    ImportThread(new QThread(this))
{
    setupUi(this);
    setupTypeComboBox();

    //Move the importer to another thread
    Importer->setThread(ImportThread);

    //Connect the importer up
    connect(Importer, SIGNAL(finished()), SLOT(importerFinishedRunning()));
    connect(Importer, SIGNAL(stopped()), SLOT(importerCanceled()));

    connect(SurvexSelectionModel, SIGNAL(selectionChanged(QItemSelection,QItemSelection)), SLOT(updateCurrentItem(QItemSelection,QItemSelection)));
    connect(ImportButton, SIGNAL(clicked()), SLOT(import()));

    SurvexTreeView->setModel(Model);
    SurvexTreeView->setSelectionModel(SurvexSelectionModel);
    SurvexTreeView->setHeaderHidden(true);
    SurvexTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    TypeComboBox->setEnabled(false);

    splitter->setStretchFactor(1, 4);

    setWindowTitle("Survex Importer");
}

cwImportSurvexDialog::~cwImportSurvexDialog() {
    delete Importer;
}

/**
  \brief Start to the dialog to import
  */
void cwImportSurvexDialog::open() {
    if(Region == NULL) {
        QMessageBox box(QMessageBox::Critical, "Broke Sauce!", "Oops, the programer has made a mistake and Cavewhere can't open Survex Import Dialog.");
        box.exec();
        return;
    }

    QSettings settings;
    QString lastFile = settings.value(ImportSurvexKey).toString();

    QFileDialog* dialog = new QFileDialog(NULL, "Import Survex", lastFile, "Survex *.svx");
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->open(this, SLOT(setSurvexFile(QString)));
}

/**
  \brief The survex file that'll be opened
  */
void cwImportSurvexDialog::setSurvexFile(QString filename) {
    QSettings settings;
    settings.setValue(ImportSurvexKey, filename);

    //The root filename
    FullFilename = filename;

    //Show a progress dialog
    cwTaskProgressDialog* progressDialog = new cwTaskProgressDialog(this);
    progressDialog->setTask(Importer);
    progressDialog->setAttribute(Qt::WA_DeleteOnClose, true);
    progressDialog->show();

    //Run the importer on another thread
    QMetaObject::invokeMethod(Importer, "setSurvexFile", Q_ARG(QString, filename));
    Importer->start();
}


void cwImportSurvexDialog::changeEvent(QEvent *e)
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
void cwImportSurvexDialog::resizeEvent(QResizeEvent* event) {
    QDialog::resizeEvent(event);

    QString cuttOffText =  FileLabel->fontMetrics().elidedText(FullFilename, Qt::ElideMiddle, FileLabel->width());
    FileLabel->setText(cuttOffText);
}



/**
  \brief Sets up the combo box for this dialog
  */
void cwImportSurvexDialog::setupTypeComboBox() {
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

    for(int i = 0; i < NumberOfItems; i++) {
        TypeComboBox->addItem(QString());
    }

    TypeComboBox->setItemIcon((int)NoImportItem, QIcon(dontImportIcon));
    TypeComboBox->setItemText((int)NoImportItem, cwSurvexBlockData::importTypeToString(cwSurvexBlockData::NoImport));
    TypeComboBox->setItemIcon((int)CaveItem, QIcon(caveIcon));
    TypeComboBox->setItemText((int)CaveItem, cwSurvexBlockData::importTypeToString(cwSurvexBlockData::Cave));
    TypeComboBox->setItemIcon((int)TripItem, QIcon(tripIcon));
    TypeComboBox->setItemText((int)TripItem, cwSurvexBlockData::importTypeToString(cwSurvexBlockData::Trip));
    TypeComboBox->setCurrentIndex(-1);

    connect(TypeComboBox, SIGNAL(activated(int)), SLOT(setType(int)));
}

/**
  \brief This will go through the cwSurvexGlobalData and see if it's possible to import it

  The main purpose of this dialog is to check for name collision for stations in a cave
  */
void cwImportSurvexDialog::updateImportErrors() {

}

/**
  \brief Updates the import warning label next to the import button
  */
void cwImportSurvexDialog::updateImportWarningLabel() {
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
void cwImportSurvexDialog::updateCurrentItem(QItemSelection selected, QItemSelection /*deselected*/) {
    QModelIndexList selectedIndexes = selected.indexes();
    if(selectedIndexes.size() == 1) {
        //Only one item selected
        QModelIndex index = selectedIndexes.first();
        cwSurvexBlockData* block = Model->toBlockData(index);
        if(block != NULL) {

            //Set the index of the combo box
            TypeItem item = importTypeToTypeItem(block->importType());
            TypeComboBox->setCurrentIndex(item);

            //Set the text for the combo box
            QString name = block->name();
            QString noImportStrting = QString("%1 %2").arg(cwSurvexBlockData::importTypeToString(cwSurvexBlockData::NoImport)).arg(name);
            QString caveString  = QString("%1 is a %2").arg(name).arg(cwSurvexBlockData::importTypeToString(cwSurvexBlockData::Cave));
            QString tripString = QString("%1 is a %2").arg(name).arg(cwSurvexBlockData::importTypeToString(cwSurvexBlockData::Trip));

            TypeComboBox->setItemText((int)NoImportItem, noImportStrting);
            TypeComboBox->setItemText((int)CaveItem, caveString);
            TypeComboBox->setItemText((int)TripItem, tripString);
            TypeComboBox->setEnabled(true);
            return;
        }
    }

    TypeComboBox->setEnabled(false);
    TypeComboBox->setCurrentIndex(-1);
}

/**
  \brief Sets the type for the currently select item(s)

  If multiple items are selected then
  */
void cwImportSurvexDialog::setType(int index) {

    QModelIndexList selectedIndexes = SurvexSelectionModel->selectedIndexes();

    foreach(QModelIndex currentIndex, selectedIndexes) {
        cwSurvexBlockData* block = Model->toBlockData(currentIndex);

        if(block != NULL) {
            cwSurvexBlockData::ImportType importType = (cwSurvexBlockData::ImportType)typeItemToImportType((TypeItem)index);
            block->setImportType(importType);
        }
    }
}


/**
  \brief Converts a typeItem to a cwSurvexBlockData::ImportType
  */
int cwImportSurvexDialog::typeItemToImportType(TypeItem typeItem) const {
    switch(typeItem) {
    case NoImportItem:
        return cwSurvexBlockData::NoImport;
    case CaveItem:
        return cwSurvexBlockData::Cave;
    case TripItem:
        return cwSurvexBlockData::Trip;
    default:
        return -1;
    }
}

    /**
      \brief Converts a cwSurvexBlockData::ImportType to a typeItem
      */
cwImportSurvexDialog::TypeItem cwImportSurvexDialog::importTypeToTypeItem(int type) const {
    switch(type) {
    case cwSurvexBlockData::NoImport:
        return NoImportItem;
    case cwSurvexBlockData::Cave:
        return CaveItem;
    case cwSurvexBlockData::Trip:
        return TripItem;
    }
    return (cwImportSurvexDialog::TypeItem)-1;
}

/**
  \brief Tries to import
  */
void cwImportSurvexDialog::import() {
    cwSurvexGlobalData* globalData = Importer->data();
    if(!globalData->caves().isEmpty()) {
        Region->addCaves(globalData->caves());
    }
    accept();
}

/**
  \brief Called when the importer has finished

  All the data has been parsed out of the importer
  */
void cwImportSurvexDialog::importerFinishedRunning() {
    Model->setSurvexData(Importer->data());

    //Cutoff to long text
    QString cutOffText = FileLabel->fontMetrics().elidedText(FullFilename, Qt::ElideMiddle, FileLabel->width());
    FileLabel->setText(cutOffText);

    //Load the error list view
    SurvexErrorListWidget->addItems(Importer->errors());

    //Update the error / warning label at the bottom
    updateImportWarningLabel();

    ImportThread->quit();
    show();
}

/**
  \brief Called if the importer has been canceled by the user
  */
void cwImportSurvexDialog::importerCanceled() {
    ImportThread->quit();
    close();
}
