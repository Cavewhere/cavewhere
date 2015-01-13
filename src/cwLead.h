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

//Our includes
#include "cwUnits.h"

class cwLeadData;

class cwLead
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

private:
    QSharedDataPointer<cwLeadData> data;
};

#endif // CWLEAD_H
