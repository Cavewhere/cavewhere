#ifndef CWTEXTURECOMPRESSIONJOB_H
#define CWTEXTURECOMPRESSIONJOB_H

//Qt includes
#include <QMutex>
#include <QPromise>

//Std includes
#include <memory>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwFutureManagerToken.h"

/**
 * The tracked "Compressing textures" job covering every UASTC encode of one
 * pipeline run.
 *
 * A run creates one job and hands shared handles to its workers. The job
 * reaches the job list the moment the first encode starts, so a run that finds
 * every texture already in the .cw_cache shows nothing at all, and it finishes
 * once the last handle is dropped. Progress counts encodes: the range is what
 * the run has started so far, since a cache miss is only discovered by the
 * worker that hits it.
 *
 * Every member is safe to call from a cwConcurrent worker thread.
 */
class CAVEWHERE_LIB_EXPORT cwTextureCompressionJob
{
public:
    using Ptr = std::shared_ptr<cwTextureCompressionJob>;

    /**
     * Marks one texture encode as running for as long as it lives. A null
     * handle makes it a no-op, so callers wrap their encode the same way
     * whether or not the run is tracked.
     */
    class CAVEWHERE_LIB_EXPORT Encode
    {
    public:
        explicit Encode(const Ptr& job);
        ~Encode();

        Encode(const Encode&) = delete;
        Encode& operator=(const Encode&) = delete;

    private:
        Ptr m_job;
    };

    static Ptr create(const cwFutureManagerToken& token);

    ~cwTextureCompressionJob();

    cwTextureCompressionJob(const cwTextureCompressionJob&) = delete;
    cwTextureCompressionJob& operator=(const cwTextureCompressionJob&) = delete;

private:
    explicit cwTextureCompressionJob(const cwFutureManagerToken& token);

    void beginEncode();
    void endEncode();

    //! Call with m_mutex held
    void publishProgress();

    cwFutureManagerToken m_token;
    QMutex m_mutex;
    QPromise<void> m_promise;
    int m_encodesStarted = 0;
    int m_encodesFinished = 0;
};

#endif // CWTEXTURECOMPRESSIONJOB_H
