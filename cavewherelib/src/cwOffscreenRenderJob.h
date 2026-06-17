#ifndef CWOFFSCREENRENDERJOB_H
#define CWOFFSCREENRENDERJOB_H

//Qt includes
#include <QImage>
#include <QPromise>

//Our includes
#include "cwOffscreenRenderParameters.h"

/**
 * @brief Internal pairing of render parameters with the promise that fulfils them.
 *
 * cwScene::renderOffscreen() creates one job per request: it copies the caller's
 * cwOffscreenRenderParameters and owns the QPromise<QImage> whose QFuture it hands
 * back. The job is shared_ptr-managed because it crosses the GUI→render thread
 * boundary (drained in cwRhiScene::synchroize) and must outlive the async GPU
 * texture read-back that fulfils the promise. Never exposed to callers — the
 * promise stays producer-side, so the result channel can only be observed (or
 * cancelled) through the returned QFuture.
 */
struct cwOffscreenRenderJob {
    cwOffscreenRenderParameters parameters;
    QPromise<QImage> promise;
};

#endif // CWOFFSCREENRENDERJOB_H
