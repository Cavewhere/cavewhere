#pragma once

//Monad result
#include <Monad/Result.h>

//Qt includes
#include <QDir>
#include <QQmlEngine>

class cwResultDir : public Monad::Result<QDir>
{
    Q_GADGET
    QML_VALUE_TYPE(cwResultDir);

public:
    cwResultDir()  {}
    explicit cwResultDir(const QString& errorMessage, int errorCode = Unknown) :
                Monad::Result<QDir>(errorMessage, errorCode)
                {}

    cwResultDir(const QDir& value):
    Monad::Result<QDir>(value)
    { }

    cwResultDir(const QDir& value, const QString& warningMessage):
        Monad::Result<QDir>(value, warningMessage)
    { }
};

