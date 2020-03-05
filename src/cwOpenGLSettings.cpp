//Our includes
#include "cwOpenGLSettings.h"

//Qt includes
#include <QtGlobal>
#include <QCoreApplication>
#include <QThread>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QSet>
#include <QByteArray>
#include <QString>

cwOpenGLSettings* cwOpenGLSettings::Singleton = nullptr;

cwOpenGLSettings::cwOpenGLSettings(QObject* parent) :
    QObject(parent)
{

}

void cwOpenGLSettings::setDXT1Compression(bool useDXT1Compression) {
    if(DXT1Compression != useDXT1Compression) {
        Q_ASSERT(thread() == QThread::currentThread());
        DXT1Compression = useDXT1Compression;
        emit useDXT1CompressionChanged();
    }
}

void cwOpenGLSettings::setMipmaps(bool useMipmaps) {
    if(Mipmaps != useMipmaps) {
        Q_ASSERT(thread() == QThread::currentThread());
        Mipmaps = useMipmaps;
        emit useMipmapsChanged();
    }
}

void cwOpenGLSettings::setNativeTextRendering(bool useNativeTextRendering) {
    if(NativeTextRendering != useNativeTextRendering) {
        Q_ASSERT(thread() == QThread::currentThread());
        NativeTextRendering = useNativeTextRendering;
        emit useNativeTextRenderingChanged();
    }
}

void cwOpenGLSettings::setRendererType(cwOpenGLSettings::Renderer rendererType) {
    if(RendererType != rendererType) {
        Q_ASSERT(thread() == QThread::currentThread());
        RendererType = rendererType;
        emit rendererTypeChanged();
    }
}

void cwOpenGLSettings::setUseAnisotropy(bool useAnisotropy) {
    if(UseAnisotropy != useAnisotropy) {
        Q_ASSERT(thread() == QThread::currentThread());
        UseAnisotropy = useAnisotropy;
        emit useAnisotropyChanged();
    }
}

void cwOpenGLSettings::setMagFilter(MagFilter magFilter) {
    if(mMagFilter != magFilter) {
        Q_ASSERT(thread() == QThread::currentThread());
        mMagFilter = magFilter;
        emit magFilterChanged();
    }
}

void cwOpenGLSettings::setMinFilter(MinFilter minFilter) {
    if(mMinFilter != minFilter) {
        Q_ASSERT(thread() == QThread::currentThread());
        mMinFilter = minFilter;
        emit minFilterChanged();
    }
}

QVector<cwOpenGLSettings::Renderer> cwOpenGLSettings::supportedRenders() const
{
#ifdef Q_OS_WIN
    return {Auto,
            Angles,
            Destop,
            Software};
#else
    return {Auto,
                Destop,
                Software}
#endif
}

void cwOpenGLSettings::initialize()
{
    if(!Singleton) {
        Q_ASSERT(QThread::currentThread() == QCoreApplication::instance()->thread());
        Singleton = new cwOpenGLSettings(QCoreApplication::instance());

        auto surface = std::make_unique<QOffscreenSurface>();
        surface->create();

        auto context = std::make_unique<QOpenGLContext>();
        context->create();
        bool contextOkay = context->makeCurrent(surface.get());
        if(contextOkay) {
            auto glFunctions = std::make_unique<QOpenGLFunctions>();
            glFunctions->initializeOpenGLFunctions();

            Singleton->Vendor = QString::fromLocal8Bit(QByteArray(reinterpret_cast<const char*>(glFunctions->glGetString(GL_VENDOR))));
            Singleton->Version = QString::fromLocal8Bit(QByteArray(reinterpret_cast<const char*>(glFunctions->glGetString(GL_VERSION))));
            Singleton->ShadingVersion = QString::fromLocal8Bit(QByteArray(reinterpret_cast<const char*>(glFunctions->glGetString(GL_SHADING_LANGUAGE_VERSION))));
            Singleton->OpenGLRenderer = QString::fromLocal8Bit(QByteArray(reinterpret_cast<const char*>(glFunctions->glGetString(GL_RENDERER))));

            auto extentions = context->extensions();
            std::transform(extentions.begin(), extentions.end(), std::back_inserter(Singleton->Extentions),
                           [](const QByteArray& extention){
                return QString::fromLocal8Bit(extention);
            });
        } else {
            Singleton->Version = "Unknown, couldn't create context";
        }
    }
}

cwOpenGLSettings *cwOpenGLSettings::instance()
{
    return Singleton;
}


void cwOpenGLSettings::setDXT1Algorithm(DXT1Algorithm dxt1Algorithm) {
    if(mDXT1Algorithm != dxt1Algorithm) {
        mDXT1Algorithm = dxt1Algorithm;
        emit dxt1AlgorithmChanged();
    }
}
