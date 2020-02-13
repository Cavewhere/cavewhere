#ifndef CWREGIONLOADRESULT_H
#define CWREGIONLOADRESULT_H

//Qt includes
#include <QSharedDataPointer>

//Our includes
#include "cwCavingRegion.h"
#include <cwError.h>

class cwRegionLoadResultData;

class cwRegionLoadResult
{
public:
    cwRegionLoadResult();
    cwRegionLoadResult(const cwRegionLoadResult &);
    cwRegionLoadResult &operator=(const cwRegionLoadResult &);
    ~cwRegionLoadResult();

    void addError(const cwError& error);
    void setErrors(const QList<cwError>& errors);
    QList<cwError> errors() const;

    void setCavingRegion(const cwCavingRegionPtr& region);
    cwCavingRegionPtr cavingRegion() const;

private:
    QSharedDataPointer<cwRegionLoadResultData> data;
};

#endif // CWREGIONLOADRESULT_H
