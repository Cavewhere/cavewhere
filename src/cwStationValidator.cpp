/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our include
#include "cwStationValidator.h"

//Qt includes
#include <QRegularExpressionValidator>

cwStationValidator::cwStationValidator(QObject *parent) :
    cwValidator(parent)
{
    setErrorText("Oops, you haven't entered a valid station name. <br> Station names need to be combination of <b> letters and numbers </b>");
}

QValidator::State cwStationValidator::validate( QString & input, int & pos ) const {
    if(input.isEmpty()) {
        return QValidator::Acceptable;
    }

    QRegularExpressionValidator validator;

    validator.setRegularExpression(validCharactersRegex());
    return validator.validate(input, pos);
}

int cwStationValidator::validate( QString input ) const {
    int pos = 0;
    return (int)validate(input, pos);
}

/**
 * @brief cwStationValidator::validCharactersRegex
 * @return Retruns the regex that makes up a valid station name
 */
QRegularExpression cwStationValidator::validCharactersRegex()
{
   return QRegularExpression("(?:[a-zA-Z0-9]|-|_)+");
}

/**
 * @brief cwStationValidator::invalidCharactersRegex
 * @return Return the regex that will match invalid station character names
 *
 * This is the inverse of validCharactersRegex()
 */
QRegularExpression cwStationValidator::invalidCharactersRegex()
{
    return QRegularExpression("((?![a-zA-Z0-9]|-|_).)*");
}
