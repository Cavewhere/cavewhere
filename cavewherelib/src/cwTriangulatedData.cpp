/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwTriangulatedData.h"

cwTriangulatedData::cwTriangulatedData() :
    Data(new PrivateData)
{
}



/**
 * @brief cwTriangulatedData::isNull
 * @return Returns null if all the parameters are empty.
 */
bool cwTriangulatedData::isNull() const
{
    return !Data->croppedImage->isValid() &&
           Data->scrapGeometry.isEmpty();
}
