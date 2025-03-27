#ifndef CWREADING_H
#define CWREADING_H

//Our include
#include <QObject>
#include <QQmlEngine>

class cwReading {
    Q_GADGET
    QML_VALUE_TYPE(cwReading)

    Q_PROPERTY(QString value MEMBER m_value READ value WRITE setValue)
    Q_PROPERTY(int state READ state)

public:
    cwReading() {}
    cwReading(const QString& value) : m_value(value) {}
    explicit cwReading(double value) {
        fromDouble(value);
    }

    QString value() const { return m_value; }
    void setValue(const QString &value);

    int state() const { return m_state; }

    double toDouble(bool* ok = nullptr) const {
        return m_value.toDouble(ok);
    }
    void fromDouble(double value) {
        //Be very careful changing this, with will cause many conversion issue if precision is too low.
        setValue(QString::number(value, 'g', 6));
    }

    bool operator==(const cwReading& other) const {
        return m_value == other.m_value;
        //Don't check state, because it's based on m_value
    }
    bool operator!=(const cwReading& other) {
        return !operator==(other);
    }

protected:
    cwReading(int state) : m_state(state) { }
    void setState(int state) {m_state = state;}

private:
    virtual void updateState() { };

    QString m_value;
    int m_state = -1; //State is assigned by subclass in updateState()
};



#endif // CWREADING_H
