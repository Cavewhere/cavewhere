#ifndef CWCOMPASSREADING_H
#define CWCOMPASSREADING_H

//Our include
#include "cwReading.h"
#include "CaveWhereLibExport.h"

//Qt includes
#include <QObject>
#include <QQmlEngine>

class CAVEWHERE_LIB_EXPORT cwCompassReading : public cwReading {
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

    cwCompassReading() : cwReading(static_cast<int>(State::Empty)) {}
    cwCompassReading(const QString& value) : cwReading(value) {
        cwCompassReading::updateState();
    }
    explicit cwCompassReading(double value) : cwReading(value) {
        cwCompassReading::updateState();
    }

    State state() const { return static_cast<State>(cwReading::state()); }

private:
    void updateState();

    // State m_state;
};

#endif // CWCOMPASSREADING_H
