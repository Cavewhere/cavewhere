//Our includes
#include "cwClinoValidator.h"

//Qt includes
#include <QDoubleValidator>
#include <QRegExp>

cwClinoValidator::cwClinoValidator(QObject *parent) :
    cwValidator(parent)
{
    setErrorText("Oops, you haven't entered a valid <i>clino reading</i>. <br> <i>Clino readings</i> need to be a <b> number between 90 and -90 </b>; or <b>keywords up or down</b>");
}

QValidator::State cwClinoValidator::validate ( QString & input, int & pos ) const {
    if(input.isEmpty()) {
        return QValidator::Acceptable;
    }

    QDoubleValidator doubleValidator;
    doubleValidator.setTop(90);
    doubleValidator.setBottom(-90);
    doubleValidator.setNotation(QDoubleValidator::StandardNotation);
    QValidator::State state = doubleValidator.validate(input, pos);

    if(state == QValidator::Invalid) {
        QRegExpValidator upDownValidator;
        QRegExp regexp("up|down", Qt::CaseInsensitive);
        upDownValidator.setRegExp(regexp);
        return upDownValidator.validate(input, pos);
    }

    return state;
}

int cwClinoValidator::validate( QString input ) const {
    int pos = 0;
    return (int)validate(input, pos);
}
