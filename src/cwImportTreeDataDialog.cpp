/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwImportTreeDataDialog.h"
#include "cwTreeDataImporterModel.h"
#include "cwTreeDataImporter.h"
#include "cwGlobalIcons.h"
#include "cwTreeImportDataNode.h"
#include "cwTreeImportData.h"
#include "cwCavingRegion.h"
#include "cwTaskProgressDialog.h"
#include "cwStringListErrorModel.h"
#include "cwGlobalUndoStack.h"

//Qt includes
#include <QFileSystemModel>
#include <QItemSelectionModel>
#include <QPixmapCache>
#include <QMessageBox>
#include <QDebug>


cwImportTreeDataDialog::cwImportTreeDataDialog(Names names, cwTreeDataImporter* importer, cwCavingRegion* region, QWidget *parent) :
    QDialog(parent),
    Region(region),
    Model(new cwTreeDataImporterModel(this)),
    Importer(importer),
    SurvexSelectionModel(new QItemSelectionModel(Model, this))
{
    setupUi(this);
    tabWidget->setTabText(tabWidget->indexOf(SurvexErrorsWidget), QApplication::translate("cwImportTreeDataDialog",
                                                                                          names.errorsLabel.toLocal8Bit().constData(), 0));

    setupTypeComboBox();

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

    SurvexErrorListView->setWordWrap(true);
    //SurvexErrorListView->setUniformItemSizes(true);

    setWindowTitle(names.windowTitle);
}

cwImportTreeDataDialog::~cwImportTreeDataDialog() {
    Importer->stop();
    Importer->waitToFinish();
    delete Importer;
}

/**
  \brief Start to the dialog to import
  */
void cwImportTreeDataDialog::open() {
    if(Region == nullptr) {
        QMessageBox box(QMessageBox::Critical, "Broke Sauce!", "Oops, the programer has made a mistake and Cavewhere can't open Survex Import Dialog.");
        box.exec();
        return;
    }
}

/**
  \brief The survex file that'll be opened
  */
void cwImportTreeDataDialog::setInputFiles(QStringList filenames) {
    //The root filename
    FullFilename = filenames.isEmpty() ? "" : filenames[0];

    //Show a progress dialog
    cwTaskProgressDialog* progressDialog = new cwTaskProgressDialog(this);
    progressDialog->setTask(Importer);
    progressDialog->setAttribute(Qt::WA_DeleteOnClose, true);
    progressDialog->show();

    //Run the importer on another thread
    QMetaObject::invokeMethod(Importer, "setInputFiles", Q_ARG(QStringList, filenames));
    Importer->start();
}


void cwImportTreeDataDialog::changeEvent(QEvent *e)
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
void cwImportTreeDataDialog::resizeEvent(QResizeEvent* event) {
    QDialog::resizeEvent(event);

    QString cuttOffText =  FileLabel->fontMetrics().elidedText(FullFilename, Qt::ElideMiddle, FileLabel->width());
    FileLabel->setText(cuttOffText);
}



/**
  \brief Sets up the combo box for this dialog
  */
void cwImportTreeDataDialog::setupTypeComboBox() {
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
    TypeComboBox->setItemText((int)NoImportItem, cwTreeImportDataNode::importTypeToString(cwTreeImportDataNode::NoImport));
    TypeComboBox->setItemIcon((int)CaveItem, QIcon(caveIcon));
    TypeComboBox->setItemText((int)CaveItem, cwTreeImportDataNode::importTypeToString(cwTreeImportDataNode::Cave));
    TypeComboBox->setItemIcon((int)TripItem, QIcon(tripIcon));
    TypeComboBox->setItemText((int)TripItem, cwTreeImportDataNode::importTypeToString(cwTreeImportDataNode::Trip));
    TypeComboBox->setCurrentIndex(-1);

    connect(TypeComboBox, SIGNAL(activated(int)), SLOT(setType(int)));
}

/**
  \brief This will go through the cwSurvexGlobalData and see if it's possible to import it

  The main purpose of this dialog is to check for name collision for stations in a cave
  */
void cwImportTreeDataDialog::updateImportErrors() {

}

/**
  \brief Updates the import warning label next to the import button
  */
void cwImportTreeDataDialog::updateImportWarningLabel() {
    QStringList errors;
    errors << Importer->parseErrors();
    errors << Importer->importErrors();
    int numberWarnings = 0;
    int numberErrors = 0;
    foreach(QString error, errors) {
        if(error.contains("error", Qt::CaseInsensitive)) {
            numberErrors++;
        } else if(error.contains("warning", Qt::CaseInsensitive)) {
            numberWarnings++;
        }
    }

    QString message;
    if(numberErrors > 0) {
        message.append(QString("<b>Errors: %1</b> ").arg(numberErrors));
    }
    if(numberWarnings > 0) {
        message.append(QString("Warnings: %2").arg(numberWarnings));
    }
    WarningLabel->setText(message);
}


/**
  \brief Updates this view with the current items that are selected
  */
void cwImportTreeDataDialog::updateCurrentItem(QItemSelection selected, QItemSelection /*deselected*/) {
    QModelIndexList selectedIndexes = selected.indexes();
    if(selectedIndexes.size() == 1) {
        //Only one item selected
        QModelIndex index = selectedIndexes.first();
        cwTreeImportDataNode* block = Model->toNode(index);
        if(block != nullptr) {

            //Set the index of the combo box
            TypeItem item = importTypeToTypeItem(block->importType());
            TypeComboBox->setCurrentIndex(item);

            //Set the text for the combo box
            QString name = block->name();
            QString noImportStrting = QString("%1 %2").arg(cwTreeImportDataNode::importTypeToString(cwTreeImportDataNode::NoImport)).arg(name);
            QString caveString  = QString("%1 is a %2").arg(name).arg(cwTreeImportDataNode::importTypeToString(cwTreeImportDataNode::Cave));
            QString tripString = QString("%1 is a %2").arg(name).arg(cwTreeImportDataNode::importTypeToString(cwTreeImportDataNode::Trip));

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
void cwImportTreeDataDialog::setType(int index) {

    QModelIndexList selectedIndexes = SurvexSelectionModel->selectedIndexes();

    foreach(QModelIndex currentIndex, selectedIndexes) {
        cwTreeImportDataNode* block = Model->toNode(currentIndex);

        if(block != nullptr) {
            cwTreeImportDataNode::ImportType importType = (cwTreeImportDataNode::ImportType)typeItemToImportType((TypeItem)index);
            block->setImportType(importType);
        }
    }
}


/**
  \brief Converts a typeItem to a cwSurvexBlockData::ImportType
  */
int cwImportTreeDataDialog::typeItemToImportType(TypeItem typeItem) const {
    switch(typeItem) {
    case NoImportItem:
        return cwTreeImportDataNode::NoImport;
    case CaveItem:
        return cwTreeImportDataNode::Cave;
    case TripItem:
        return cwTreeImportDataNode::Trip;
    default:
        return -1;
    }
}

    /**
      \brief Converts a cwSurvexBlockData::ImportType to a typeItem
      */
cwImportTreeDataDialog::TypeItem cwImportTreeDataDialog::importTypeToTypeItem(int type) const {
    switch(type) {
    case cwTreeImportDataNode::NoImport:
        return NoImportItem;
    case cwTreeImportDataNode::Cave:
        return CaveItem;
    case cwTreeImportDataNode::Trip:
        return TripItem;
    }
    return (cwImportTreeDataDialog::TypeItem)-1;
}

/**
  \brief Tries to import
  */
void cwImportTreeDataDialog::import() {
    cwTreeImportData* globalData = Importer->data();

    if(!globalData->caves().isEmpty()) {
        beginUndoMacro("Import survex");
        Region->addCaves(globalData->caves());
        endUndoMacro();
    }
    accept();
}

/**
  \brief Called when the importer has finished

  All the data has been parsed out of the importer
  */
void cwImportTreeDataDialog::importerFinishedRunning() {
//    //Move the importer back to this thread
//    Importer->setThread(thread(), Qt::BlockingQueuedConnection);

    //Get the importer's data
    Model->setTreeImportData(Importer->data());

    //Cutoff to long text
    QString cutOffText = FileLabel->fontMetrics().elidedText(FullFilename, Qt::ElideMiddle, FileLabel->width());
    FileLabel->setText(cutOffText);

    //Load the parse error list view
    cwStringListErrorModel* parsingErrorsModel = new cwStringListErrorModel(this);
    parsingErrorsModel->setStringList(Importer->parseErrors());
    SurvexErrorListView->setModel(parsingErrorsModel);

    //Load the import error list view
    cwStringListErrorModel* importErrorsModel = new cwStringListErrorModel(this);
    importErrorsModel->setStringList(Importer->importErrors());
    ImportErrorListView->setModel(importErrorsModel);

    //Update the error / warning label at the bottom
    updateImportWarningLabel();

    show();
}

/**
  \brief Called if the importer has been canceled by the user
  */
void cwImportTreeDataDialog::importerCanceled() {
    Importer->waitToFinish();
    close();
}
