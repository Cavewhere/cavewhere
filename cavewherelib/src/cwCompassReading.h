#ifndef CWCOMPASSREADING_H
#define CWCOMPASSREADING_H

//Our include
#include <QObject>
#include <QQmlEngine>

class cwCompassReading {
    Q_GADGET
    QML_VALUE_TYPE(cwCompassReading)

    Q_PROPERTY(QString value MEMBER m_value READ value WRITE setValue)
    Q_PROPERTY(State state READ state)
public:
    enum State {
        Valid = 0,   // Data is valid (convertible to a number)
        Empty = 1,   // No data entered
        Invalid = 2  // Data is not a valid number
    };
    Q_ENUM(State)

    cwCompassReading() : m_state(Empty) {}
    cwCompassReading(const QString& value) : m_value(value) {
        updateState();
    }

    QString value() const { return m_value; }
    void setValue(const QString &value) {
        m_value = value;
        updateState();
    }

    State state() const { return m_state; }

private:
    void updateState();

    QString m_value;
    State m_state;
};

#endif // CWCOMPASSREADING_H
