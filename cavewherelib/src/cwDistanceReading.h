#ifndef CWDISTANCEREADING_H
#define CWDISTANCEREADING_H

//Qt includes
#include <QObject>
#include <QQmlEngine>

//Our includes
#include "cwReading.h"

class cwDistanceReading : public cwReading {
    Q_GADGET
    QML_VALUE_TYPE(cwDistanceReading)

    Q_PROPERTY(State state READ state)
public:
    enum class State : int {
        Valid = 0,   // Data is valid (convertible to a number)
        Empty = 1,   // No data entered
        Invalid = 2  // Data is not a valid number
    };
    Q_ENUM(State)

    cwDistanceReading() : m_state(State::Empty) {
    }
    cwDistanceReading(const QString& value) : cwReading(value) {
        cwDistanceReading::updateState();
    }
    explicit cwDistanceReading(double value) : cwReading(value) {
        cwDistanceReading::updateState();
    }

    State state() const { return m_state; }

private:
    void updateState();

    State m_state;
};

#endif // CWDISTANCEREADING_H
