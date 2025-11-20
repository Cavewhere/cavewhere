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
    void addErrors(const QList<cwError>& errors);
    void setErrors(const QList<cwError>& errors);
    QList<cwError> errors() const;

    void setCavingRegion(const cwCavingRegionData& region);
    cwCavingRegionData cavingRegion() const;

    void setFileVersion(int fileVersion);
    int fileVersion() const;

    void setFileName(const QString& filename);
    QString filename() const;

    void setIsTempFile(bool isTempFile);
    bool isTempFile() const;

private:
    QSharedDataPointer<cwRegionLoadResultData> data;
};

#endif // CWREGIONLOADRESULT_H
