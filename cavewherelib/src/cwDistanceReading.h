#ifndef CWDISTANCEREADING_H
#define CWDISTANCEREADING_H

#include <QObject>
#include <QQmlEngine>

class cwDistanceReading {
    Q_GADGET
    QML_VALUE_TYPE(cwDistanceReading)

    Q_PROPERTY(QString value MEMBER m_value READ value WRITE setValue)
    Q_PROPERTY(State state READ state)
public:
    enum State {
        Valid = 0,   // Data is valid (convertible to a number)
        Empty = 1,   // No data entered
        Invalid = 2  // Data is not a valid number
    };
    Q_ENUM(State)

    cwDistanceReading() : m_value(""), m_state(Empty) {
    }
    cwDistanceReading(const QString& value) : m_value(value) {
        updateState();
    }
    explicit cwDistanceReading(double value) {
        fromDouble(value);
    }

    QString value() const { return m_value; }
    void setValue(const QString &value) {
        m_value = value;
        updateState();
    }
    double toDouble() const
    {
        Q_ASSERT(m_state == Valid);
        return m_value.toDouble();
    }
    void fromDouble(double value) {
        setValue(QString::number(value, 'g', 2));
    }

    State state() const { return m_state; }

private:
    void updateState();

    QString m_value;
    State m_state;
};

#endif // CWDISTANCEREADING_H
