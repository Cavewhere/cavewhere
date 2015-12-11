#include "cwTreeDataImporter.h"
#include "cwTreeImportData.h"

cwTreeDataImporter::cwTreeDataImporter(QObject *parent) : cwTask(parent)
{

}

bool cwTreeDataImporter::hasErrors()
{
    return false;
}

QStringList cwTreeDataImporter::errors()
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

void cwTreeDataImporter::setSurvexFile(QString filename)
{
    Q_UNUSED(filename);
}

void cwTreeDataImporter::runTask()
{

}
