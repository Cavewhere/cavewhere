/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSVGREADER_H
#define CWSVGREADER_H

#include <QByteArray>
#include <QImage>

class cwSvgReader
{
public:
    static bool isSvg(const QByteArray& format);
    static QImage toImage(QByteArray& data, const QByteArray& format);
};

#endif // CWSVGREADER_H
