#ifndef CWLENGTH_H
#define CWLENGTH_H

//Qt includes
#include <QObject>
#include <QSharedDataPointer>
#include <QSharedData>
#include <QStringList>

class cwLength : public QObject
{
    Q_OBJECT

    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(Unit unit READ unit WRITE setUnit NOTIFY unitChanged)
    Q_PROPERTY(QStringList unitNames READ unitNames NOTIFY unitNamesChanged)

    Q_ENUMS(Unit)
public:
    enum Unit {
        in,
        ft,
        m,
        mm,
        cm,
        km,
        Unitless
    };


    explicit cwLength(QObject *parent = 0);
    cwLength(double value, Unit unit, QObject* parent = 0);
    cwLength(const cwLength& other);
    const cwLength& operator =(const cwLength& other);

    double value() const;
    void setValue(double value);

    Unit unit() const;
    void setUnit(Unit unit);

    QStringList unitNames();
    QString unitName(Unit unit);

    cwLength convertTo(Unit to) const;
    static double convert(double value, Unit from, Unit to);
    static double toMeters(Unit unit);

signals:
    void valueChanged();
    void unitChanged();
    void unitNamesChanged();

private:

    class PrivateData : public QSharedData {
    public:
        PrivateData() :
            LengthUnit(cwLength::Unitless),
            Value(0.0)
        {
        }

        PrivateData(double value, Unit unit) :
            LengthUnit(unit),
            Value(value)
            {
        }

        cwLength::Unit LengthUnit;
        double Value;
    };

    QSharedDataPointer<PrivateData> Data;

};


/**
    Get's the value of the length
  */
inline double cwLength::value() const {
    return Data->Value;
}

/**
    Get's the units of the value
  */
inline cwLength::Unit cwLength::unit() const {
    return Data->LengthUnit;
}

#endif // CWLENGTH_H
