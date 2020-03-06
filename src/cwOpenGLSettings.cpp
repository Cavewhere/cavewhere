//Our includes
#include "cwOpenGLSettings.h"
#include "cwDXT1Compresser.h"
#include "cwOpenGLUtils.h"
#include "cwAsyncFuture.h"

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
#include <QSettings>

//Stc3 decompression
#include "s3tc.h"

cwOpenGLSettings* cwOpenGLSettings::Singleton = nullptr;

const QString cwOpenGLSettings::BaseKey = "renderingSettings.%1";
const QString cwOpenGLSettings::CrashWhenTestingKey = "crashWhenTesting";
const QString cwOpenGLSettings::RendererKey = "renderer";

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
            GPU,
            Software};
#else
    return {Auto,
                GPU,
                Software};
#endif
}

void cwOpenGLSettings::initialize()
{
    if(!Singleton) {

        QSettings settings;
        settings.setValue(BaseKey.arg("testing"), true);

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

            Singleton->DXT1Supported = context->extensions().contains("GL_EXT_texture_compression_dxt1");
            Singleton->AnisotropySupported = context->extensions().contains("GL_EXT_texture_filter_anisotropic");

            if(Singleton->DXT1Supported) {
                Singleton->GPUGeneratedDXT1Supported = testGPU_DXT1();
            }
        } else {
            Singleton->Version = "Unknown, couldn't create context";
        }
    }
}

cwOpenGLSettings *cwOpenGLSettings::instance()
{
    return Singleton;
}

cwOpenGLSettings::Renderer cwOpenGLSettings::perviousRenderer()
{
    QSettings settings;
    bool crashWhenTesting = settings.value(BaseKey.arg(CrashWhenTestingKey), false).toBool();
    if(crashWhenTesting) {
        return Software;
    }

    return static_cast<Renderer>(settings.value(cwOpenGLSettings::BaseKey.arg(cwOpenGLSettings::RendererKey), Auto).toInt());
}

bool cwOpenGLSettings::testGPU_DXT1()
{
    QImage testImage("://icons/dxt1TestImage.png");

    cwDXT1Compresser openGLCompresser;
    auto openGLFuture = openGLCompresser.openglCompression({testImage});

    cwDXT1Compresser squishCompresser;
    auto squishFuture = squishCompresser.squishCompression({testImage});

    cwAsyncFuture::waitForFinished(openGLFuture);
    cwAsyncFuture::waitForFinished(squishFuture);

    auto openGLResult = openGLFuture.result();
    auto squishResult = squishFuture.result();

    if(openGLResult.size != squishResult.size) {
        return false;
    }

    if(openGLResult.data.size() != squishResult.data.size()) {
        return false;
    }

    QSize size = openGLResult.size;

    QVector<unsigned int> openGLImage(size.width() * size.height(), 0);
    QVector<unsigned int> squishImage(size.width() * size.height(), 0);

    qDebug() << "Data: " << openGLResult.data.size() << squishResult.data.size() << squishImage.size();

    s3tc::BlockDecompressImageDXT1(size.width(), size.height(),
                             reinterpret_cast<const unsigned char*>(openGLResult.data.constData()), openGLImage.data());

    s3tc::BlockDecompressImageDXT1(size.width(), size.height(),
                                   reinterpret_cast<const unsigned char*>(squishResult.data.constData()), squishImage.data());

    double sumOfSquares = 0;
    for(int r = 0; r < size.height(); r++) {
        for(int c = 0; c < size.width(); c++) {
            auto pixelIndex = r * size.width() + c;
            auto openGLPixel = openGLImage.at(pixelIndex);
            auto squishPixel = squishImage.at(pixelIndex);

            auto openGL_rgba = cwOpenGLUtils::toQRgba(openGLPixel);
            auto squish_rgba = cwOpenGLUtils::toQRgba(squishPixel);

            sumOfSquares += cwOpenGLUtils::fuzzyCompareColors(openGL_rgba, squish_rgba);
        }
    }

    auto diff = sqrt(sumOfSquares) / static_cast<double>(size.height() * size.width());

    return diff < 0.01;
}


void cwOpenGLSettings::setDXT1Algorithm(DXT1Algorithm dxt1Algorithm) {
    if(mDXT1Algorithm != dxt1Algorithm) {
        mDXT1Algorithm = dxt1Algorithm;
        emit dxt1AlgorithmChanged();
    }
}

cwOpenGLSettings::Renderer cwOpenGLSettings::rendererType() const {
    return RendererType;
}
