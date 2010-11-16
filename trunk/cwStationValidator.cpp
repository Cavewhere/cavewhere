//Our include
#include "cwStationValidator.h"

//Qt includes
#include <QRegExpValidator>

cwStationValidator::cwStationValidator(QObject *parent) :
    cwValidator(parent)
{
}

QValidator::State cwStationValidator::validate( QString & input, int & pos ) const {
    QRegExpValidator validator;
    QRegExp regExp("\\w+(\\.\\w+)*");
    validator.setRegExp(regExp);
    return validator.validate(input, pos);
}

int cwStationValidator::validate( QString input ) const {
    int pos = 0;
    return (int)validate(input, pos);
}
