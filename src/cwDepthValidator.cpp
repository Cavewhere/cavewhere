/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwDepthValidator.h"

//Qt includes
#include <QDoubleValidator>
#include <QRegExp>

cwDepthValidator::cwDepthValidator(QObject *parent) :
    cwValidator(parent)
{
    setErrorText("Oops, you haven't entered a valid <i>Depth reading</i>. <br> <i>Depth readings</i> need to be a ");
}

QValidator::State cwDepthValidator::validate ( QString & input, int & pos ) const {
    if(input.isEmpty()) {
        return QValidator::Acceptable;
    }

    QDoubleValidator doubleValidator;
    doubleValidator.setNotation(QDoubleValidator::StandardNotation);
    QValidator::State state = doubleValidator.validate(input, pos);

    switch(state) {
    case QValidator::Invalid: {
        QRegExpValidator upDownValidator;
        QRegExp regexp("up|down", Qt::CaseInsensitive);
        upDownValidator.setRegExp(regexp);
        return upDownValidator.validate(input, pos);
    }
    case QValidator::Acceptable: {
        //Just make sure we can convert the input
        bool okay;
        double value = input.toDouble(&okay);
        if(!okay || !check(value)) {
            //The validator is dump ... this handle use case input="5,5"
            return QValidator::Invalid;
        }
        break;
    }
    default:
        break;
    }

    return state;
}

int cwDepthValidator::validate( QString input ) const {
    int pos = 0;
    return (int)validate(input, pos);
}


