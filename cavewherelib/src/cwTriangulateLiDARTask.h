#ifndef CWTRIANGULATELIDARTASK_H
#define CWTRIANGULATELIDARTASK_H

//Our includes
#include "cwTriangulateLiDARInData.h"
#include "cwRenderGLTF.h"
#include "cwRenderTexturedItems.h"
#include "cwTextureCompressionJob.h"

//Qt includes
#include <QFuture>

//Monad includes
#include <Monad/Monad.h>

class cwTriangulateLiDARTask
{
public:
    cwTriangulateLiDARTask() = delete;

    /**
     * compressionJob surfaces the notes' KTX2 texture encodes in the job list.
     * Leaving it unset still encodes, it just goes untracked.
     */
    static QFuture<Monad::Result<QVector<cwRenderTexturedItems::Item>>> triangulate(const QList<cwTriangulateLiDARInData>& liDARs,
                                                                                    const cwTextureCompressionJob::Ptr& compressionJob = {});
    static QVector<cwRenderTexturedItems::Item> reserveRenderItems(const QVector<cw::gltf::MeshCPU>& meshes);
};

#endif // CWTRIANGULATELIDARTASK_H
