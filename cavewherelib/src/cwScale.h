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

    static double scale(const Data& data);

    //! The default paper scale for a project unit system: metric 1 cm = 2.5 m
    //! (1:250), imperial 1 in = 20 ft (1:240). Shared by the sketch map scale
    //! and a new scrap's note-transformation scale so both round the same way.
    static Data defaultData(cwUnits::UnitSystem system);

signals:
     //! The ratio moved. Relabeling both sides into other units leaves the ratio
     //! alone and stays quiet here, so geometry consumers don't rebuild for a
     //! change they can't see — use dataChanged() to track the stored units too.
     void scaleChanged();

     //! Either side's unit or value changed, ratio or not. What persistence
     //! listens to: "1 in = 20 ft" and "1 in = 6.096 m" are the same scale but
     //! not the same saved data.
     void dataChanged();

public slots:

private:
     cwLength* ScaleNumerator; //!< This is the numerator of the scale, usually 1
     cwLength* ScaleDenominator; //!< The scale denominator
     double m_scale; //!< Last emitted ratio, so scaleChanged() only fires on a real move
     bool m_settingData = false; //!< Inside setData(), where the two sides move together

     void connectLengthObjects();
     void updateScale();


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
