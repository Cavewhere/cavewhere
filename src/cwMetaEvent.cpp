/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#include "cwMetaEvent.h"

cwMetaEvent::cwMetaEvent() :
    QInputEvent(QEvent::MetaCall)
{
}
