/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwExternalCenterline.h"

cwExternalCenterline::cwExternalCenterline(QString entryFile)
    : m_entryFile(std::move(entryFile))
{
}

size_t qHash(const cwExternalCenterline& value, size_t seed) noexcept
{
    return qHash(value.entryFile(), seed);
}
