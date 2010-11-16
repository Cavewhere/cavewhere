
//Our includes
#include "cwDistanceValidator.h"

//Qt includes
#include <QDoubleValidator>

cwDistanceValidator::cwDistanceValidator(QObject *parent) :
    cwValidator(parent)
{
}

QValidator::State cwDistanceValidator::validate( QString & input, int & pos ) const {
    QDoubleValidator validator;
    validator.setBottom(0);
    return validator.validate(input, pos);
}

int cwDistanceValidator::validate( QString input ) const {
    int pos = 0;
    return (int)validate(input, pos);
}
