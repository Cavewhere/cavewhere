/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPLACEMENTRULEPARAMS_H
#define CWPLACEMENTRULEPARAMS_H

//Qt includes
#include <QMetaType>
#include <QString>

// The Uniform spacing rule's registry/displayName key. Declared once here so the
// rule's displayName() and the codec's name dispatch can't drift apart — a
// rename in only one place would otherwise silently fall back to opaque bytes.
inline QString cwUniformSpacingRuleName() { return QStringLiteral("Uniform spacing"); }

// Typed, proto-free view of a placement rule's parameters. The default member
// values ARE the documented fallbacks, so a rule reading value<T>() off a null
// or wrong-typed QVariant gets the right defaults with no presence check.
struct cwUniformSpacingParams {
    double spacingMm = 2.0;   // was cwUniformSpacingRule's kDefaultStampSpacingMm
    bool operator==(const cwUniformSpacingParams &other) const = default;
};

Q_DECLARE_METATYPE(cwUniformSpacingParams)

// The Offset stroke rule's registry/displayName key — see cwUniformSpacingRuleName
// for why it lives here rather than inline in the rule and the codec.
inline QString cwOffsetStrokeRuleName() { return QStringLiteral("Offset stroke"); }

// Signed lateral offset (paper-mm) the Offset stroke rule pushes the stroke by,
// along its left normal: + = left side, 0 = on the stroke. The 0.0 default makes
// a null or wrong-typed QVariant a no-op offset.
struct cwOffsetStrokeParams {
    double offsetMm = 0.0;
    bool operator==(const cwOffsetStrokeParams &other) const = default;
};

Q_DECLARE_METATYPE(cwOffsetStrokeParams)

#endif // CWPLACEMENTRULEPARAMS_H
