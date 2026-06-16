/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSYMBOLOGYERRORCODES_H
#define CWSYMBOLOGYERRORCODES_H

#include "Monad/Result.h"

// Stable error-type ids for symbology palette validation, stored in
// cwError::errorTypeId. Numbered off Monad::ResultBase::CustomError so they
// never collide with the generic result codes (mirrors cwLinePlotErrorCodes.h).
// Tests assert on these codes, not message prose, so messages stay editable; the
// brush editor (commit 9) keys fix-its and suppression off them.
enum class SymbologyErrorCode : int {
    TwoTerminals            = Monad::ResultBase::CustomError,       // 1024
    TwoTransformStrokes     = Monad::ResultBase::CustomError + 1,   // 1025
    NoTerminalForRules      = Monad::ResultBase::CustomError + 2,   // 1026
    StampsWithoutGenerate   = Monad::ResultBase::CustomError + 3,   // 1027
    StampsWithoutGlyph      = Monad::ResultBase::CustomError + 4,   // 1028
    MissingGlyph            = Monad::ResultBase::CustomError + 5,   // 1029
    DeadRulesUnderTrace     = Monad::ResultBase::CustomError + 6,   // 1030
    UnknownRule             = Monad::ResultBase::CustomError + 7,   // 1031
};

#endif // CWSYMBOLOGYERRORCODES_H
