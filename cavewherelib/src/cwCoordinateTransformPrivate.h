/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCOORDINATETRANSFORMPRIVATE_H
#define CWCOORDINATETRANSFORMPRIVATE_H

//
// Private header for cwCoordinateTransform. Exposes the typed PROJ Pimpl
// (cwCoordinateTransformPrivate) and a context factory to sibling .cpp
// files inside cavewherelib that need direct PROJ access (cwCRSSearchModel,
// future LAZ loader, line-plot post-processor). The public header
// cwCoordinateTransform.h deliberately doesn't include <proj.h>, so callers
// who don't need PROJ types stay free of the dependency.
//

#include "cwCoordinateTransform.h"

//PROJ includes
#include <proj.h>

//Qt includes
#include <QString>

class cwCoordinateTransformPrivate
{
public:
    PJ_CONTEXT* ctx = nullptr;
    PJ*         pj  = nullptr;
    bool        identity = false;
    QString     error;

    ~cwCoordinateTransformPrivate();

    /**
     * Build a fresh PJ_CONTEXT with the search paths configured via
     * cwCoordinateTransform::setProjSearchPaths() applied. Caller owns the
     * returned pointer and must proj_context_destroy() it. Returns nullptr
     * if PROJ refuses to allocate a context.
     */
    static PJ_CONTEXT* createContext();
};

#endif // CWCOORDINATETRANSFORMPRIVATE_H
