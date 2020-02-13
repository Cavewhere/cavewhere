
//Our includes
#include "cwRegionLoadResult.h"
#include "cwCavingRegion.h"
#include "cwError.h"

class cwRegionLoadResultData : public QSharedData
{
public:
    QList<cwError> errors;
    cwCavingRegionPtr region;
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

void cwRegionLoadResult::setErrors(const QList<cwError> &errors)
{
    data->errors = errors;
}

QList<cwError> cwRegionLoadResult::errors() const
{
    return data->errors;
}

void cwRegionLoadResult::setCavingRegion(const cwCavingRegionPtr &region)
{
    data->region = region;
}

cwCavingRegionPtr cwRegionLoadResult::cavingRegion() const
{
    return data->region;
}
