/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSymbologyName.h"

namespace cwSymbology {

bool isValidName(QStringView name)
{
    if (name.isEmpty()) {
        return false;
    }
    for (const QChar c : name) {
        const bool kebabChar = (c >= u'a' && c <= u'z')
                            || (c >= u'0' && c <= u'9')
                            || c == u'-';
        if (!kebabChar) {
            return false;
        }
    }
    // No leading/trailing hyphen — keeps the name canonical kebab-case (a bare
    // "-" or "--" is already excluded by the char check being the only allowed
    // punctuation).
    return name.front() != u'-' && name.back() != u'-';
}

}
