//Our includes
#include "cwRHIObject.h"

//Qt includes
#include <QFile>


QShader cwRHIObject::loadShader(const QString &name)
{
    QFile f(name);
    if (f.open(QIODevice::ReadOnly))
        return QShader::fromSerialized(f.readAll());

    return QShader();
}
