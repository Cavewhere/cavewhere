#ifndef CWCOMPASSREADING_H
#define CWCOMPASSREADING_H

//Our include
#include "cwReading.h"

//Qt includes
#include <QObject>
#include <QQmlEngine>

class cwCompassReading : public cwReading {
    Q_GADGET
    QML_VALUE_TYPE(cwCompassReading)

    Q_PROPERTY(State state READ state)
public:
    enum class State : int {
        Valid = 0,   // Data is valid (convertible to a number)
        Empty = 1,   // No data entered
        Invalid = 2  // Data is not a valid number
    };
    Q_ENUM(State)

    cwCompassReading() : m_state(State::Empty) {}
    cwCompassReading(const QString& value) : cwReading(value) {
        cwCompassReading::updateState();
    }
    explicit cwCompassReading(double value) : cwReading(value) {
        cwCompassReading::updateState();
    }

    State state() const { return m_state; }

private:
    void updateState();

    State m_state;
};

#endif // CWCOMPASSREADING_H
