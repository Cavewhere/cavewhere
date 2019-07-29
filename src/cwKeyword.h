#ifndef CWKEYWORD_H
#define CWKEYWORD_H

//Qt includes
#include <QObject>

//Our includes
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwKeyword
{
    Q_GADGET

    Q_PROPERTY(QString key READ key WRITE setKey)
    Q_PROPERTY(QString value READ value WRITE setValue)

public:
    cwKeyword() = default;
    cwKeyword(const QString key, QString value);

    QString key() const;
    void setKey(QString key);

    QString value() const;
    void setValue(QString value);

    bool isValid() const;

    bool operator==(const cwKeyword& other) const;
    bool operator!=(const cwKeyword& other) const;

private:
    QString Key; //!<
    QString Value; //!<
};

/**
* Returns the key of the key word
*/
inline QString cwKeyword::key() const {
    return Key;
}

/**
* Sets the key of the keyword
*/
inline void cwKeyword::setKey(QString key) {
    Key = key;
}

/**
* Returns the value of the keyword, aka the word
*/
inline QString cwKeyword::value() const {
    return Value;
}

/**
* Sets the value of the keyword, aka the word
*/
inline void cwKeyword::setValue(QString value) {
    Value = value;
}

inline bool cwKeyword::isValid() const
{
    return !Key.isEmpty() && !Value.isEmpty();
}

inline bool cwKeyword::operator!=(const cwKeyword &other) const
{
    return !operator==(other);
}
#endif // CWKEYWORD_H
