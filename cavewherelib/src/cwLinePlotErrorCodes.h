/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLINEPLOTERRORCODES_H
#define CWLINEPLOTERRORCODES_H

#include "Monad/Result.h"

enum class LinePlotErrorCode : int {
    Cancelled        = Monad::ResultBase::CustomError,       // 1024
    ExportFailed     = Monad::ResultBase::CustomError + 1,   // 1025
    CavernFailed     = Monad::ResultBase::CustomError + 2,   // 1026
    ParseFailed      = Monad::ResultBase::CustomError + 3,   // 1027
    GeometryFailed   = Monad::ResultBase::CustomError + 4,   // 1028
    ValidationFailed = Monad::ResultBase::CustomError + 5,   // 1029
};

#endif // CWLINEPLOTERRORCODES_H
