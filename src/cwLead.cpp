/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#include "cwLead.h"

class cwLeadData : public QSharedData
{
public:

};

cwLead::cwLead() : data(new cwLeadData)
{

}

cwLead::cwLead(const cwLead &rhs) : data(rhs.data)
{

}

cwLead &cwLead::operator=(const cwLead &rhs)
{
    if (this != &rhs)
        data.operator=(rhs.data);
    return *this;
}

cwLead::~cwLead()
{

}

