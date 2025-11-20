/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWSCALEHELPER_H
#define CWSCALEHELPER_H

//Qt includes
#include <QObject>
#include <QQmlEngine>

//Our includes
#include "cwGlobals.h"
#include "cwLength.h"

class CAVEWHERE_LIB_EXPORT cwScale : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Scale)

    Q_PROPERTY(double scale READ scale WRITE setScale NOTIFY scaleChanged)
    Q_PROPERTY(cwLength* scaleNumerator READ scaleNumerator CONSTANT)
    Q_PROPERTY(cwLength* scaleDenominator READ scaleDenominator CONSTANT)

public:
    struct Data {
        cwLength::Data scaleNumerator;
        cwLength::Data scaleDenominator;
    };

    explicit cwScale(QObject *parent = 0);

    cwLength* scaleNumerator() const;
    cwLength* scaleDenominator() const;

    void setScale(double scale);
    double scale() const;

    void setData(const Data& data);
    Data data() const;

signals:
     void scaleChanged();

public slots:

private:
     cwLength* ScaleNumerator; //!< This is the numerator of the scale, usually 1
     cwLength* ScaleDenominator; //!< The scale denominator

     void connectLengthObjects();


};

inline cwLength *cwScale::scaleNumerator() const
{
    return ScaleNumerator;
}

inline cwLength *cwScale::scaleDenominator() const
{
    return ScaleDenominator;
}

#endif // CWSCALEHELPER_H
