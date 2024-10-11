/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


//Our includes
#include "cwDistanceValidator.h"

//Qt includes
#include <QDoubleValidator>

cwDistanceValidator::cwDistanceValidator(QObject *parent) :
    cwValidator(parent)
{
    setErrorText("Oops, you haven't entered a <i>distance</i>. <br> <i>Distances</i> need to be a <b>number</b> that's <b>greater or equal to 0.0</b>");
}

QValidator::State cwDistanceValidator::validate( QString & input, int & pos ) const {
    if(input.isEmpty()) {
        return QValidator::Acceptable;
    }

    QDoubleValidator validator;
    validator.setBottom(0);
    validator.setNotation(QDoubleValidator::StandardNotation);
    QValidator::State state = validator.validate(input, pos);

    if(state == QValidator::Acceptable) {
        //Just make sure we can convert the input
        bool okay;
        double value = input.toDouble(&okay);
        if(!okay || !check(value)) {
            //The validator is dumb ... this handle use case input="5,5"
            return QValidator::Invalid;
        }
    }

    return state;
}

int cwDistanceValidator::validate( QString input ) const {
    int pos = 0;
    return static_cast<int>(validate(input, pos));
}

