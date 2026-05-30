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

namespace {

// Native CaveWhere station names: alphanumerics, hyphen, underscore.
// Anchored implicitly by QRegularExpressionValidator (which requires the
// whole input to match for Acceptable). Locked here so the regex literal
// lives in one place across the four static accessors.
const char kNativeValidCharacters[] = "(?:[a-zA-Z0-9]|-|_)+";
const char kNativeInvalidCharacters[] = "((?![a-zA-Z0-9]|-|_).)*";

// External station names: native set + dot (cavern namespacing,
// "block.station") + literal space (Walls "feet inches" survey-name
// shape). Note that the character class deliberately puts hyphen at the
// end so it does not start a range.
const char kExternalValidCharacters[] = "[a-zA-Z0-9_. \\-]+";
const char kExternalInvalidCharacters[] = "((?![a-zA-Z0-9_. \\-]).)*";

} // namespace

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

    validator.setRegularExpression(m_external
                                       ? externalStationCharactersRegex()
                                       : validCharactersRegex());
    return validator.validate(input, pos);
}

int cwStationValidator::validate( QString input ) const {
    int pos = 0;
    return (int)validate(input, pos);
}

void cwStationValidator::setExternal(bool external) {
    if(m_external == external) {
        return;
    }
    m_external = external;
    emit externalChanged();
}

/**
 * @brief cwStationValidator::validCharactersRegex
 * @return Anchored regex that matches a whole valid native station
 * name (alphanumerics, hyphen, underscore).
 *
 * Anchored via QRegularExpression::anchoredPattern so a direct
 * .match("A.1").hasMatch() call on the returned regex returns false
 * for inputs that contain disallowed characters. Without the anchor,
 * a consumer asking "is this string a valid station name?" would get
 * a false positive on any string that contained ANY valid character.
 * QRegularExpressionValidator (used by validate()) auto-anchors as
 * well; the extra wrap is a no-op for that consumer.
 */
QRegularExpression cwStationValidator::validCharactersRegex()
{
   return QRegularExpression(
       QRegularExpression::anchoredPattern(QString::fromLatin1(kNativeValidCharacters)));
}

/**
 * @brief cwStationValidator::invalidCharactersRegex
 * @return Return the regex that will match invalid station character names
 *
 * This is the inverse of validCharactersRegex() and is used to strip /
 * highlight offending characters via globalMatch; intentionally
 * unanchored.
 */
QRegularExpression cwStationValidator::invalidCharactersRegex()
{
    return QRegularExpression(QString::fromLatin1(kNativeInvalidCharacters));
}

/**
 * @brief cwStationValidator::externalStationCharactersRegex
 * @return Anchored regex of valid characters inside an externally-backed
 * cave or trip's station name. Same as validCharactersRegex() plus dot
 * and space.
 */
QRegularExpression cwStationValidator::externalStationCharactersRegex()
{
    return QRegularExpression(
        QRegularExpression::anchoredPattern(QString::fromLatin1(kExternalValidCharacters)));
}

/**
 * @brief cwStationValidator::externalInvalidCharactersRegex
 * @return The inverse of externalStationCharactersRegex(), suitable for
 * stripping or highlighting offending characters via globalMatch.
 */
QRegularExpression cwStationValidator::externalInvalidCharactersRegex()
{
    return QRegularExpression(QString::fromLatin1(kExternalInvalidCharacters));
}
