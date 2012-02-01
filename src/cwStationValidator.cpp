//Our include
#include "cwStationValidator.h"

//Qt includes
#include <QRegExpValidator>

cwStationValidator::cwStationValidator(QObject *parent) :
    cwValidator(parent)
{
    setErrorText("Oops, you haven't entered a valid station name. <br> Station names need to be combination of <b> letters and numbers </b>");
}

QValidator::State cwStationValidator::validate( QString & input, int & pos ) const {
    if(input.isEmpty()) {
        return QValidator::Acceptable;
    }

    QRegExpValidator validator;
    QRegExp regExp("\\w+(\\.\\w+)*");
    validator.setRegExp(regExp);
    return validator.validate(input, pos);
}

int cwStationValidator::validate( QString input ) const {
    int pos = 0;
    return (int)validate(input, pos);
}
