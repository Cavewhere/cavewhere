/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWLEAD_H
#define CWLEAD_H

//Qt includes
#include <QSharedDataPointer>
#include <QPointF>
#include <QSizeF>
#include <QUuid>

//Our includes
#include "cwUnits.h"
#include "cwGlobals.h"

class cwLeadData;

class CAVEWHERE_LIB_EXPORT cwLead
{
public:
    cwLead();
    cwLead(const cwLead &);
    cwLead &operator=(const cwLead &);
    ~cwLead();

    void setPositionOnNote(QPointF point);
    QPointF positionOnNote() const;

    void setDescription(QString desciption);
    QString desciption() const;

    void setSize(QSizeF size);
    QSizeF size() const;

    void setCompleted(bool compeleted);
    bool completed() const;

    void setId(const QUuid& id);
    QUuid id() const;

private:
    QSharedDataPointer<cwLeadData> data;
};

#endif // CWLEAD_H
