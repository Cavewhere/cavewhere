//Our includes
#include "cwClinoValidator.h"

//Qt includes
#include <QDoubleValidator>
#include <QRegExp>

cwClinoValidator::cwClinoValidator(QObject *parent) :
    cwValidator(parent)
{
}

QValidator::State cwClinoValidator::validate ( QString & input, int & pos ) const {
    QDoubleValidator doubleValidator;
    doubleValidator.setTop(90);
    doubleValidator.setBottom(-90);
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
