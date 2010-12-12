#ifndef CWIMPORTSURVEXDIALOG_H
#define CWIMPORTSURVEXDIALOG_H

//Our includes
#include "ui_cwImportSurvexDialog.h"
class cwSurvexImporterModel;
class cwSurvexImporter;

class cwImportSurvexDialog : public QDialog, private Ui::cwImportSurvexDialog
{
    Q_OBJECT

public:
    explicit cwImportSurvexDialog(QWidget *parent = 0);

public slots:
    void setSurvexFile(QString filename);

protected:
    void changeEvent(QEvent *e);

private:
    cwSurvexImporterModel* Model;
    cwSurvexImporter* Importer;

};

#endif // CWIMPORTSURVEXDIALOG_H
