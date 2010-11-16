//Our includes
#include "cwCompassValidator.h"

//Qt includes
#include <QDoubleValidator>

cwCompassValidator::cwCompassValidator(QObject *parent) :
    cwValidator(parent)
{
}

QValidator::State cwCompassValidator::validate( QString & input, int & pos ) const {
    QDoubleValidator validator;
    validator.setTop(360);
    validator.setBottom(0);
    return validator.validate(input, pos);
}

int cwCompassValidator::validate( QString input ) const {
    int pos = 0;
    return (int)validate(input, pos);
}
