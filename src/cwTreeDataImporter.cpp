#include "cwTreeDataImporter.h"
#include "cwSurvexGlobalData.h"

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

cwSurvexGlobalData* cwTreeDataImporter::data()
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
