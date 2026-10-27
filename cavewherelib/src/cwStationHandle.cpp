/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwStationHandle.h"

size_t qHash(const cwStationHandle& value, size_t seed) noexcept
{
    return qHashMulti(seed, static_cast<int>(value.scope()), value.containerId(), value.tail());
}
