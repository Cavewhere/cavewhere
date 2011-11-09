#ifndef CWNOTETRANFORMATION_H
#define CWNOTETRANFORMATION_H

//Qt includes
#include <QSharedData>
#include <QSharedDataPointer>
#include <QObject>
#include <QPointF>

//Our includes
class cwLength;

/**
  This class is useful for allowing automatic station referencing.  This holds the page of note's
  scale (example 40:1 and rotation that rotates the page such that north is up, ie. parallel the y axis.

  The scale can be set throught the scaleNumerator : scaleDenominator.  When one of those change, the
  unitless scale will also change.
  */
class cwNoteTranformation : public QObject
{
    Q_OBJECT

    Q_PROPERTY(double scale READ scale NOTIFY scaleChanged)
    Q_PROPERTY(double northUp READ northUp WRITE setNorthUp NOTIFY northUpChanged)
//    Q_PROPERTY(double pixelsPerInch READ pixelsPerInch WRITE setPixelsPerInch NOTIFY pixelsPerInchChanged)
    Q_PROPERTY(cwLength* scaleNumerator READ scaleNumerator NOTIFY scaleNumeratorChanged)
    Q_PROPERTY(cwLength* scaleDenominator READ scaleDenominator NOTIFY scaleDenominatorChanged)

public:
    cwNoteTranformation(QObject* parent = 0);
    cwNoteTranformation(const cwNoteTranformation& other);
    const cwNoteTranformation& operator =(const cwNoteTranformation& other);

    cwLength* scaleDenominator() const;
    cwLength* scaleNumerator() const;
    double scale() const;

    double northUp() const;
    void setNorthUp(double degrees);

//    double pixelsPerInch() const;
//    void setPixelsPerInch(double pixelsPerInch);

    Q_INVOKABLE double calculateNorth(QPointF noteP1, QPointF noteP2) const;

signals:
    void scaleChanged();
    void northUpChanged();
//    void pixelsPerInchChanged();
    void scaleNumeratorChanged();
    void scaleDenominatorChanged();
private:
    class PrivateData : public QSharedData {
    public:
        PrivateData() {
            PixelsPerInch = 0.0;
            North = 0.0;
        }

        double PixelsPerInch; //The number pixels per inch in the notes
        double North;  //The north of the scarp, with page of notes without rotation, in degrees
     };

    QSharedDataPointer<PrivateData> Data;
    cwLength* ScaleNumerator; //!< This is the numerator of the scale, usually 1
    cwLength* ScaleDenominator; //!< The scale denominator
};
//Q_DECLARE_METATYPE(cwNoteTranformation*)

/**
  In degrees, the rotation of the page of notes such that north is aligned with the y axis.
  */
inline double cwNoteTranformation::northUp() const {
    return Data->North;
}

///**
//  Get's the resolution of the page of notes
//  */
//inline double cwNoteTranformation::pixelsPerInch() const {
//    return Data->PixelsPerInch;
//}

/**
Gets scaleDenominator
*/
inline cwLength* cwNoteTranformation::scaleDenominator() const {
    return ScaleDenominator;
}

/**
Gets scaleNumerator
*/
inline cwLength* cwNoteTranformation::scaleNumerator() const {
    return ScaleNumerator;
}

#endif // CWNOTETRANFORMATION_H
