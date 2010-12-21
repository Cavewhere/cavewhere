#ifndef CWIMPORTSURVEXDIALOG_H
#define CWIMPORTSURVEXDIALOG_H

//Our includes
#include "ui_cwImportSurvexDialog.h"
class cwSurvexImporterModel;
class cwSurvexImporter;
class cwCavingRegion;

//Qt includes
class QItemSelectionModel;
class QAction;
#include <QItemSelection>

class cwImportSurvexDialog : public QDialog, private Ui::cwImportSurvexDialog
{
    Q_OBJECT

public:
    explicit cwImportSurvexDialog(cwCavingRegion* region, QWidget *parent = 0);

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

private slots:
    void updateCurrentItem(QItemSelection selected, QItemSelection deselected);
    void setType(int index);

    TypeItem importTypeToTypeItem(int type) const;
    int typeItemToImportType(TypeItem typeItem) const;

    void import();

};

#endif // CWIMPORTSURVEXDIALOG_H
