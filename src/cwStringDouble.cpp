/**************************************************************************
**
**    Copyright (C) 2016 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwStringDouble.h"

cwStringDouble::cwStringDouble() :
    QString(QString::number(0.0))
{
}

cwStringDouble::cwStringDouble(const cwStringDouble &str) :
    QString(str)
{

}

cwStringDouble::cwStringDouble(const QString str) :
    QString(str)
{

}

cwStringDouble::cwStringDouble(double value) :
    QString(QString::number(value))
{

}

cwStringDouble &cwStringDouble::operator=(const cwStringDouble &other)
{
    *this = static_cast<const QString&>(other);
    return *this;
}

cwStringDouble &cwStringDouble::operator=(const QString &other)
{
    QString* thisString = static_cast<QString*>(this);
    *thisString = other;
    return *this;
}

cwStringDouble &cwStringDouble::operator=(double other)
{
    *this = QString::number(other);
    return *this;
}
