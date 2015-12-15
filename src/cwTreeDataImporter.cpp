#include "cwTreeDataImporter.h"
#include "cwTreeImportData.h"

cwTreeDataImporter::cwTreeDataImporter(QObject *parent) : cwTask(parent)
{

}

bool cwTreeDataImporter::hasParseErrors()
{
    return false;
}

QStringList cwTreeDataImporter::parseErrors()
{
    return QStringList();
}

bool cwTreeDataImporter::hasImportErrors()
{
    return false;
}

QStringList cwTreeDataImporter::importErrors()
{
    return QStringList();
}

QString cwTreeDataImporter::lastImport()
{
    return QString();
}

cwTreeImportData* cwTreeDataImporter::data()
{
    return nullptr;
}

void cwTreeDataImporter::runTask()
{

}
