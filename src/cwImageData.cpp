/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwImageData.h"

//cwImageData::cwImageData(int id) {
//    Id = id;
//}

cwImageData::cwImageData() :
    d(new PrivateData())
{
    d->DotsPerMeter = 0;
}

cwImageData::cwImageData(QSize size, int dotsPerMeter, const QByteArray& format, const QByteArray& image) :
    d(new PrivateData())
{
    d->Size = size;
    d->DotsPerMeter = dotsPerMeter;
    d->Format = format;
    d->Data = image;
}

cwImageData::cwImageData(const cwImageData &other) : d(other.d)
{

}

cwImageData& cwImageData::operator=(const cwImageData &other)
{
    if (this != &other)
        d.operator=(other.d);
    return *this;
}

