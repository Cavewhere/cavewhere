//Our includes
#include "cwRHIObject.h"
#include "cwDebug.h"

//Qt includes
#include <QFile>


QShader cwRHIObject::loadShader(const QString &name)
{
    QFile f(name);
    if (f.open(QIODevice::ReadOnly)) {
        return QShader::fromSerialized(f.readAll());
    } else {
        qDebug() << "Can't read shader:" << name << LOCATION;
    }

    return QShader();
}
