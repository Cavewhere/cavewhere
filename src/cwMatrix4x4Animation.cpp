//Our include
#include "cwMatrix4x4Animation.h"
#include "cwDebug.h"

//Qt includes
#include <QMatrix4x4>

cwMatrix4x4Animation::cwMatrix4x4Animation(QObject *parent) :
    QVariantAnimation(parent)
{
    //This is
    setEasingCurve(QEasingCurve::InExpo);
}

/**
 * @brief cwMatrix4x4Animation::interpolated
 * @param from
 * @param to
 * @param progress
 * @return
 */
QVariant cwMatrix4x4Animation::interpolated(const QVariant &from, const QVariant &to, qreal progress) const
{
    if(!from.canConvert<QMatrix4x4>() ||
        !to.canConvert<QMatrix4x4>()) {
        qDebug() << "Can't animate because from or to isn't a QMatrix4x4" << LOCATION;
    }

    QMatrix4x4 fromMatrix = from.value<QMatrix4x4>();
    QMatrix4x4 toMatrix = to.value<QMatrix4x4>();

    QMatrix4x4 result;

    for(int i = 0; i < 16; i++) {
        float fromValue = fromMatrix.data()[i];
        float toValue = toMatrix.data()[i];

        result.data()[i] = fromValue + (toValue - fromValue) * progress;
    }

    return result;
}
