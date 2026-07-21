/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWRENDEROBJECTID_H
#define CWRENDEROBJECTID_H

//Qt includes
#include <QtGlobal>
#include <QDebug>

// Strong, opaque render-object identity. A quint64 at runtime (zero overhead),
// but a distinct type so it can't be confused with other quint64s or used in
// arithmetic. Correlates a render object with its cwRHIObject in place of the
// raw `this` pointer, whose address the allocator can recycle (issue #512).
//
// Qt provides qHash() for scoped enums, so this works directly as a QHash key.
enum class cwRenderObjectId : quint64 {};

inline QDebug operator<<(QDebug debug, cwRenderObjectId id)
{
    return debug << static_cast<quint64>(id);
}

#endif // CWRENDEROBJECTID_H
