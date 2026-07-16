/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWBILLBOARDID_H
#define CWBILLBOARDID_H

//Qt includes
#include <QtGlobal>

// Strong, opaque identity for a billboard in cwRenderBillboards. A uint32_t at
// runtime (zero overhead), but a distinct type so it can't be confused with a
// label index or any other integer, or used in arithmetic. Mirrors
// cwRenderObjectId. Zero-initialises to an invalid id (the first minted id is 1),
// so a default-constructed cwBillboardId means "no billboard".
//
// Qt provides qHash() for scoped enums and the standard library provides
// std::hash for enumeration types, so this works directly as a QHash or
// std::unordered_map key.
enum class cwBillboardId : uint32_t {};

#endif // CWBILLBOARDID_H
