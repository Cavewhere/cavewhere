#ifndef CWDPIVALIDATOR_H
#define CWDPIVALIDATOR_H

#include <QQmlEngine>
#include "cwDistanceValidator.h"

class cwDPIValidator : public cwDistanceValidator
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DPIValidator)
public:
    explicit cwDPIValidator(QObject *parent = nullptr);

protected:
    bool rangeCheck(double value) const {
        return value > 0.0;
    }
};

#endif // CWDPIVALIDATOR_H
