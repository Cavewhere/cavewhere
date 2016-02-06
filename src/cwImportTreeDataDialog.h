/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWIMPORTSURVEXDIALOG_H
#define CWIMPORTSURVEXDIALOG_H

//Our includes
#include "ui_cwImportTreeDataDialog.h"
#include "cwUndoer.h"
class cwTreeDataImporterModel;
class cwTreeDataImporter;
class cwCavingRegion;

//Qt includes
class QItemSelectionModel;
class QAction;
#include <QItemSelection>
#include <QDebug>

class cwImportTreeDataDialog :
        public QDialog,
        public cwUndoer,
        private Ui::cwImportTreeDataDialog
{
    Q_OBJECT

public:
    struct Names {
        QString windowTitle;
        QString errorsLabel;
    };

    explicit cwImportTreeDataDialog(Names names, cwTreeDataImporter* importer, cwCavingRegion* region, QWidget *parent = 0);
    virtual ~cwImportTreeDataDialog();

    void open();

public slots:
    void setInputFiles(QStringList filenames);

protected:
    void changeEvent(QEvent *e);
    void resizeEvent(QResizeEvent *event);

private:
    enum TypeItem {
        Invalid = -1,
        NoImportItem,
        CaveItem,
        TripItem,
        NumberOfItems
    };

    //Where the data will be exported to
    cwCavingRegion* Region;

    //Data stuff
    QString FullFilename;
    cwTreeDataImporterModel* Model;
    cwTreeDataImporter* Importer;
    QItemSelectionModel* SurvexSelectionModel;

    //For threading ithe importer
    QThread* ImportThread;

    void setupTypeComboBox();

    void updateImportErrors();

    void updateImportWarningLabel();

private slots:
    void updateCurrentItem(QItemSelection selected, QItemSelection deselected);
    void setType(int index);

    TypeItem importTypeToTypeItem(int type) const;
    int typeItemToImportType(TypeItem typeItem) const;

    void import();
    void importerFinishedRunning();
    void importerCanceled();

    void updateImportButton(QModelIndex begin, QModelIndex end);

};

#endif // CWIMPORTSURVEXDIALOG_H
