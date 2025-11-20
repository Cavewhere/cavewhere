
//Our includes
#include "cwRegionLoadResult.h"
#include "cwCavingRegion.h"
#include "cwError.h"

class cwRegionLoadResultData : public QSharedData
{
public:
    QList<cwError> errors;
    cwCavingRegionData region;
    // cwCavingRegionPtr region;
    int version = 0;
    QString filename;
    bool isTempFile = false;
};

cwRegionLoadResult::cwRegionLoadResult() : data(new cwRegionLoadResultData)
{

}

cwRegionLoadResult::cwRegionLoadResult(const cwRegionLoadResult &rhs) : data(rhs.data)
{

}

cwRegionLoadResult &cwRegionLoadResult::operator=(const cwRegionLoadResult &rhs)
{
    if (this != &rhs)
        data.operator=(rhs.data);
    return *this;
}

cwRegionLoadResult::~cwRegionLoadResult()
{

}

void cwRegionLoadResult::addError(const cwError &error)
{
    data->errors.append(error);
}

void cwRegionLoadResult::addErrors(const QList<cwError> &errors)
{
    data->errors.append(errors);
}

void cwRegionLoadResult::setErrors(const QList<cwError> &errors)
{
    data->errors = errors;
}

QList<cwError> cwRegionLoadResult::errors() const
{
    return data->errors;
}

void cwRegionLoadResult::setCavingRegion(const cwCavingRegionData &region)
{
    data->region = region;
}

cwCavingRegionData cwRegionLoadResult::cavingRegion() const
{
    return data->region;
}

void cwRegionLoadResult::setFileVersion(int version)
{
    data->version = version;
}

int cwRegionLoadResult::fileVersion() const
{
    return data->version;
}

void cwRegionLoadResult::setFileName(const QString &filename)
{
    data->filename = filename;
}

QString cwRegionLoadResult::filename() const
{
    return data->filename;
}

void cwRegionLoadResult::setIsTempFile(bool isTempFile)
{
    data->isTempFile = isTempFile;
}

bool cwRegionLoadResult::isTempFile() const
{
    return data->isTempFile;
}
