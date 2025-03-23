#ifndef CWCLINOREADING_H
#define CWCLINOREADING_H

//Our includes
#include "cwReading.h"

//Qt includes
#include <QObject>
#include <QQmlEngine>

class cwClinoReading : public cwReading {
    Q_GADGET
    QML_VALUE_TYPE(cwClinoReading)

    Q_PROPERTY(State state READ state)
public:

    enum class State : int {
        Valid = 0,   // Data is valid (convertible to a number)
        Empty = 1,   // No data entered
        Down = 2,    // Data equals "down"
        Up = 3,       // Data equals "up"
        Invalid = 4, // Data is neither numeric nor one of the keywords
    };

    Q_ENUM(State)

    cwClinoReading() : m_state(State::Empty) {}
    cwClinoReading(const QString& value) : cwReading(value) {
        cwClinoReading::updateState();
    }
    explicit cwClinoReading(double value) : cwReading(value) {
        cwClinoReading::updateState();
    }

    State state() const { return m_state; }

private:
    void updateState();

    State m_state;
};

#endif // CWCLINOREADING_H
