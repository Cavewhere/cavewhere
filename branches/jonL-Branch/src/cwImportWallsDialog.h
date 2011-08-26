#ifndef CWIMPORTWALLSDIALOG_H
#define CWIMPORTWALLSDIALOG_H

//Our includes
#include "ui_cwImportWallsDialog.h"
#include "cwUndoer.h"
class cwWallsImporterModel;
class cwWallsImporter;
class cwCavingRegion;

//Qt includes
class QItemSelectionModel;
class QAction;
#include <QItemSelection>
#include <QDebug>

class cwImportWallsDialog :
        public QDialog,
        public cwUndoer,
        private Ui::cwImportWallsDialog
{
    Q_OBJECT

public:
    explicit cwImportWallsDialog(cwCavingRegion* region, QWidget *parent = 0);
    virtual ~cwImportWallsDialog();

    void open();

public slots:
    void setWallsFile(QString filename);

protected:
    void changeEvent(QEvent *e);
    void resizeEvent(QResizeEvent *event);

private:
    static const QString ImportWallsKey;

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
    cwWallsImporterModel* Model;
    cwWallsImporter* Importer;
    QItemSelectionModel* WallsSelectionModel;

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

};

#endif // CWIMPORTWALLSDIALOG_H
