/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWLEAD_H
#define CWLEAD_H

#include <QSharedDataPointer>

class cwLeadData;

class cwLead
{
public:
    cwLead();
    cwLead(const cwLead &);
    cwLead &operator=(const cwLead &);
    ~cwLead();

private:
    QSharedDataPointer<cwLeadData> data;
};

#endif // CWLEAD_H
