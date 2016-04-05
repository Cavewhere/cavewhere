/**************************************************************************
**
**    Copyright (C) 2016 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWSURVEYNETWORK_H
#define CWSURVEYNETWORK_H

//Qt includes
#include <QSharedDataPointer>

//Our includes
#include "cwGlobals.h"

class cwSurveyNetworkData;

class CAVEWHERE_LIB_EXPORT cwSurveyNetwork
{
public:
    cwSurveyNetwork();
    cwSurveyNetwork(const cwSurveyNetwork &);
    cwSurveyNetwork &operator=(const cwSurveyNetwork &);
    ~cwSurveyNetwork();

    void clear();
    void addShot(QString from, QString to);
    QStringList neighbors(QString stationName) const;

private:
    QSharedDataPointer<cwSurveyNetworkData> data;
};

#endif // CWSURVEYNETWORK_H
