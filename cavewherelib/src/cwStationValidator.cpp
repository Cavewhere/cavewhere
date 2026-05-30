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
// Locked here as a literal so the regex pattern lives in one place
// across the static accessor and the on-disk grammar comment in
// cwLinePlotTask.cpp.
const char kNativeValidCharacters[] = "(?:[a-zA-Z0-9]|-|_)+";

// External station names: native set + dot (cavern namespacing,
// "block.station") + literal space (Walls "feet inches" survey-name
// shape). Note that the character class deliberately puts hyphen at the
// end so it does not start a range.
const char kExternalValidCharacters[] = "[a-zA-Z0-9_. \\-]+";

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
 *
 * Cached behind a function-local static so the per-keystroke
 * validate() path doesn't recompile the pattern. QRegularExpression
 * is implicitly shared, so the by-value return is a refcount bump.
 */
QRegularExpression cwStationValidator::validCharactersRegex()
{
   static const QRegularExpression regex(
       QRegularExpression::anchoredPattern(QString::fromLatin1(kNativeValidCharacters)));
   return regex;
}

/**
 * @brief cwStationValidator::externalStationCharactersRegex
 * @return Anchored regex of valid characters inside an externally-backed
 * cave or trip's station name. Same as validCharactersRegex() plus dot
 * and space.
 *
 * Cached behind a function-local static; see validCharactersRegex() for
 * rationale.
 */
QRegularExpression cwStationValidator::externalStationCharactersRegex()
{
    static const QRegularExpression regex(
        QRegularExpression::anchoredPattern(QString::fromLatin1(kExternalValidCharacters)));
    return regex;
}
