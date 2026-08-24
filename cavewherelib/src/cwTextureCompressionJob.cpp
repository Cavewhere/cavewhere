//Our includes
#include "cwTextureCompressionJob.h"

namespace {
    //What the job list calls the encode, in the user's terms
    inline const QString kJobName = QStringLiteral("Compressing textures");
}

cwTextureCompressionJob::Ptr cwTextureCompressionJob::create(const cwFutureManagerToken& token)
{
    return Ptr(new cwTextureCompressionJob(token));
}

cwTextureCompressionJob::cwTextureCompressionJob(const cwFutureManagerToken& token) :
    m_token(token)
{
}

cwTextureCompressionJob::~cwTextureCompressionJob()
{
    //The last handle is gone, so the run has no encode left to report. Finished
    //rather than left to the promise's own destructor, which cancels instead.
    m_promise.finish();
}

void cwTextureCompressionJob::beginEncode()
{
    QMutexLocker locker(&m_mutex);

    m_encodesStarted++;

    if(m_encodesStarted == 1) {
        //Added on the first encode, so a run served entirely from the disk
        //cache never flashes a job
        m_promise.start();
        m_token.addJob(m_promise.future(), kJobName);
    }

    publishProgress();
}

void cwTextureCompressionJob::endEncode()
{
    QMutexLocker locker(&m_mutex);

    m_encodesFinished++;
    publishProgress();
}

void cwTextureCompressionJob::publishProgress()
{
    m_promise.setProgressRange(0, m_encodesStarted);
    m_promise.setProgressValue(m_encodesFinished);
}

cwTextureCompressionJob::Encode::Encode(const Ptr& job) :
    m_job(job)
{
    if(m_job) {
        m_job->beginEncode();
    }
}

cwTextureCompressionJob::Encode::~Encode()
{
    if(m_job) {
        m_job->endEncode();
    }
}
