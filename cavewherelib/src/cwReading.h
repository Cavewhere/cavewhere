#ifndef CWREADING_H
#define CWREADING_H

//Our include
#include <QObject>
#include <QQmlEngine>

class cwReading {
    Q_GADGET
    QML_VALUE_TYPE(cwReading)

    Q_PROPERTY(QString value MEMBER m_value READ value WRITE setValue)

public:
    cwReading() {}
    cwReading(const QString& value) : m_value(value) {}
    explicit cwReading(double value) {
        fromDouble(value);
    }

    QString value() const { return m_value; }
    void setValue(const QString &value);

    double toDouble(bool* ok = nullptr) const {
        return m_value.toDouble(ok);
    }
    void fromDouble(double value) {
        //Be very careful changing this, with will cause many conversion issue if precision is too low.
        setValue(QString::number(value, 'g', 6));
    }

    bool operator==(const cwReading& other) const {
        return m_value == other.m_value;
    }
    bool operator!=(const cwReading& other) {
        return !operator==(other);
    }

private:
    virtual void updateState() { };

    QString m_value;
};



#endif // CWREADING_H
