#include "cwWallsImportData.h"

cwWallsImportData::cwWallsImportData(QObject* parent)
    : cwTreeImportData(parent)
{

}

QList<cwCave*> cwWallsImportData::caves()
{
    return QList<cwCave*>();
}
