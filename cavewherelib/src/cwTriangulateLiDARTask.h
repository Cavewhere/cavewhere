#ifndef CWTRIANGULATELIDARTASK_H
#define CWTRIANGULATELIDARTASK_H

//Our includes
#include "cwTriangulateLiDARInData.h"
#include "cwRenderGLTF.h"
#include "cwRenderTexturedItems.h"
#include "cwProgressNode.h"
#include "asyncfuture.h"

//Qt includes
#include <QFuture>
#include <QList>

//Monad includes
#include <Monad/Monad.h>

class cwTriangulateLiDARTask
{
public:
    using Result = Monad::Result<QVector<cwRenderTexturedItems::Item>>;

    cwTriangulateLiDARTask() = delete;

    //Notes are admitted through a window bounded by the pool's thread count and
    //a decode budget. `items` holds one future per input, in input order, each
    //existing before its note is admitted; `all` settles when every note
    //settles, and canceling it stops admission and cancels every note. Call
    //from context's thread; destroying context cancels every note.
    //`progressRoot` is the run's progress tree: each note grows a node under it
    //while it works, and a null root leaves the task untracked.
    static AsyncFuture::Limited<Result> triangulate(
        QObject* context,
        const QList<cwTriangulateLiDARInData>& liDARs,
        const cwProgressNodePtr& progressRoot = {});
    static QVector<cwRenderTexturedItems::Item> reserveRenderItems(const QVector<cw::gltf::MeshCPU>& meshes);
};

#endif // CWTRIANGULATELIDARTASK_H
