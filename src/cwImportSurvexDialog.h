/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWIMPORTSURVEXDIALOG_H
#define CWIMPORTSURVEXDIALOG_H

//Our includes
#include "ui_cwImportSurvexDialog.h"
#include "cwUndoer.h"
class cwSurvexImporterModel;
class cwSurvexImporter;
class cwCavingRegion;

//Qt includes
class QItemSelectionModel;
class QAction;
#include <QItemSelection>
#include <QDebug>

class cwImportSurvexDialog :
        public QDialog,
        public cwUndoer,
        private Ui::cwImportSurvexDialog
{
    Q_OBJECT

public:
    explicit cwImportSurvexDialog(cwCavingRegion* region, QWidget *parent = 0);
    virtual ~cwImportSurvexDialog();

    void open();

public slots:
    void setSurvexFile(QString filename);

protected:
    void changeEvent(QEvent *e);
    void resizeEvent(QResizeEvent *event);

private:
    static const QString ImportSurvexKey;

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
    cwSurvexImporterModel* Model;
    cwSurvexImporter* Importer;
    QItemSelectionModel* SurvexSelectionModel;

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

};

#endif // CWIMPORTSURVEXDIALOG_H
