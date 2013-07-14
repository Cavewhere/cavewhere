/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

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
    QValidator::State state = validator.validate(input, pos);

    if(state == QValidator::Acceptable) {
        //Just make sure we can convert the input
        bool okay;
        double value = input.toDouble(&okay);
        if(!okay || !check(value)) {
            //The validator is dump ... this handle use case input="5,5"
            return QValidator::Invalid;
        }
    }

    return state;
}

int cwCompassValidator::validate( QString input ) const {
    int pos = 0;
    return (int)validate(input, pos);
}

