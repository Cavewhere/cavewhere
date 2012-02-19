//Our includes
#include "cwSurveyImportManager.h"
#include "cwImportSurvexDialog.h"

cwSurveyImportManager::cwSurveyImportManager(QObject *parent) :
    QObject(parent)
{
}

void cwSurveyImportManager::setCavingRegion(cwCavingRegion *region)
{
    if(CavingRegion != region) {
        CavingRegion = region;
    }
}



/**
  \brief Opens the survex importer dialog
  */
void cwSurveyImportManager::importSurvex() {
    cwImportSurvexDialog* survexImportDialog = new cwImportSurvexDialog(cavingRegion());
    survexImportDialog->setUndoStack(UndoStack);
    survexImportDialog->setAttribute(Qt::WA_DeleteOnClose, true);
    survexImportDialog->open();
}
