#ifndef CWIMAGEITEMRESOURCES_H
#define CWIMAGEITEMRESOURCES_H

//Our includes
#include "cwGLResources.h"
#include "cwImageTexture.h"

//Qt includes
class QOpenGLBuffer;

/**
 * @brief The cwImageItemResources class
 *
 * This class stores all the GL resources for the cwImageItem.  When the image item
 * releases it's data, deleteLater() is called on this object to release the GL resources.
 */
class cwGLImageItemResources : public cwGLResources
{
    Q_OBJECT
public:
    explicit cwGLImageItemResources();
    virtual ~cwGLImageItemResources();

    cwImageTexture* NoteTexture;
    QOpenGLBuffer GeometryVertexBuffer;

signals:
    
public slots:
    
};

#endif // CWIMAGEITEMRESOURCES_H
