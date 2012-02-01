//Our includes
#include "cwCompassValidator.h"

//Qt includes
#include <QDoubleValidator>

cwCompassValidator::cwCompassValidator(QObject *parent) :
    cwValidator(parent)
{
    setErrorText("Oops, you haven't entered a valid <i>compass reading</i>. <br><i>Compass readings<i> need to be a <b> number between 0.0 and 360.0</b>");
}

QValidator::State cwCompassValidator::validate( QString & input, int & pos ) const {
    if(input.isEmpty()) {
        return QValidator::Acceptable;
    }

    QDoubleValidator validator;
    validator.setTop(360);
    validator.setBottom(0);
    validator.setNotation(QDoubleValidator::StandardNotation);
    return validator.validate(input, pos);
}

int cwCompassValidator::validate( QString input ) const {
    int pos = 0;
    return (int)validate(input, pos);
}
