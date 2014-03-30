/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWNOTETRANFORMATION_H
#define CWNOTETRANFORMATION_H

//Qt includes
#include <QSharedData>
#include <QSharedDataPointer>
#include <QObject>
#include <QPointF>
#include <QSize>
#include <QMatrix4x4>

//Our includes
class cwLength;
class cwImageResolution;
class cwScale;

/**
  This class is useful for allowing automatic station referencing.  This holds the page of note's
  scale (example 40:1 and rotation that rotates the page such that north is up, ie. parallel the y axis.

  The scale can be set throught the scaleNumerator : scaleDenominator.  When one of those change, the
  unitless scale will also change.
  */
class cwNoteTranformation : public QObject
{
    Q_OBJECT

    Q_PROPERTY(double scale READ scale WRITE setScale NOTIFY scaleChanged)
    Q_PROPERTY(double northUp READ northUp WRITE setNorthUp NOTIFY northUpChanged)
    Q_PROPERTY(cwLength* scaleNumerator READ scaleNumerator CONSTANT)
    Q_PROPERTY(cwLength* scaleDenominator READ scaleDenominator CONSTANT)

public:
    cwNoteTranformation(QObject* parent = 0);
    cwNoteTranformation(const cwNoteTranformation& other);
    const cwNoteTranformation& operator =(const cwNoteTranformation& other);

    cwLength* scaleNumerator() const;
    cwLength* scaleDenominator() const;

    void setScale(double scale);
    double scale() const;

    double northUp() const;
    void setNorthUp(double degrees);

    Q_INVOKABLE double calculateNorth(QPointF noteP1, QPointF noteP2) const;
    Q_INVOKABLE double calculateScale(QPointF noteP1, QPointF noteP2,
                                      cwLength* length,
                                      QSize imageSize,
                                      cwImageResolution* resolution);

    QMatrix4x4 matrix() const;

signals:
    void scaleChanged();
    void northUpChanged();

private:
    double North;
    cwScale* Scale;
};
//Q_DECLARE_METATYPE(cwNoteTranformation*)

/**
  In degrees, the rotation of the page of notes such that north is aligned with the y axis.
  */
inline double cwNoteTranformation::northUp() const {
    return North;
}



#endif // CWNOTETRANFORMATION_H
