
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
    return validator.validate(input, pos);
}

int cwDistanceValidator::validate( QString input ) const {
    int pos = 0;
    return (int)validate(input, pos);
}
