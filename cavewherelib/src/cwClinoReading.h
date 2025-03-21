#ifndef CWCLINOREADING_H
#define CWCLINOREADING_H

//Qt includes
#include <QObject>
#include <QQmlEngine>

class cwClinoReading {
    Q_GADGET
    QML_VALUE_TYPE(cwClinoReading)

    Q_PROPERTY(QString value MEMBER m_value READ value WRITE setValue)
    Q_PROPERTY(State state READ state)
public:
    enum State {
        Valid = 0,   // Data is valid (convertible to a number)
        Empty = 1,   // No data entered
        Down = 2,    // Data equals "down"
        Up = 3,       // Data equals "up"
        Invalid = 4, // Data is neither numeric nor one of the keywords
    };
    Q_ENUM(State)

    cwClinoReading() : m_state(Empty) {}
    cwClinoReading(const QString& value) : m_value(value) {
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

#endif // CWCLINOREADING_H
