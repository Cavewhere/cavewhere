#ifndef CWIMPORTSURVEXDIALOG_H
#define CWIMPORTSURVEXDIALOG_H

//Our includes
#include "ui_cwImportSurvexDialog.h"
class cwSurvexImporterModel;
class cwSurvexImporter;

//Qt includes
class QItemSelectionModel;
class QAction;
#include <QItemSelection>

class cwImportSurvexDialog : public QDialog, private Ui::cwImportSurvexDialog
{
    Q_OBJECT

public:
    explicit cwImportSurvexDialog(QWidget *parent = 0);

    void import();

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

    //Data stuff
    QString FullFilename;
    cwSurvexImporterModel* Model;
    cwSurvexImporter* Importer;
    QItemSelectionModel* SurvexSelectionModel;

    void setupTypeComboBox();

private slots:
    void updateCurrentItem(QItemSelection selected, QItemSelection deselected);
    void setType(int index);

    TypeItem ImportTypeToTypeItem(int type) const;
    int TypeItemToImportType(TypeItem typeItem) const;

};

#endif // CWIMPORTSURVEXDIALOG_H
