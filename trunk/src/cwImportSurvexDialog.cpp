//Our includes
#include "cwImportSurvexDialog.h"
#include "cwSurvexImporterModel.h"
#include "cwSurvexImporter.h"

//Qt includes
#include <QFileSystemModel>

cwImportSurvexDialog::cwImportSurvexDialog(QWidget *parent) :
    QDialog(parent),
    Model(new cwSurvexImporterModel(this)),
    Importer(new cwSurvexImporter(this))
{
    setupUi(this);

    SurvexTreeView->setModel(Model);
}

/**
  \brief The survex file that'll be opened

  */
void cwImportSurvexDialog::setSurvexFile(QString filename) {
    Importer->importSurvex(filename);
    Model->setSurvexData(Importer->data());
    show();
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
