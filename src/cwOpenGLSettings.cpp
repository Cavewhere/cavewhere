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
#include <QVariant>

//Stc3 decompression
#include "s3tc.h"

cwOpenGLSettings* cwOpenGLSettings::Singleton = nullptr;

cwOpenGLSettings::cwOpenGLSettings(QObject* parent) :
    QObject(parent)
{

}

void cwOpenGLSettings::setUseDXT1Compression(bool useDXT1Compression) {
    if(DXT1Compression != useDXT1Compression) {
        Q_ASSERT(thread() == QThread::currentThread());
        DXT1Compression = useDXT1Compression;
        updateSettingsWithDevice(dXT1CompressionKey(), DXT1Compression);
        emit useDXT1CompressionChanged();
    }
}

void cwOpenGLSettings::setMipmaps(bool useMipmaps) {
    if(Mipmaps != useMipmaps) {
        Q_ASSERT(thread() == QThread::currentThread());
        Mipmaps = useMipmaps;
        updateSettingsWithDevice(mipmapsKey(), Mipmaps);
        emit useMipmapsChanged();
    }
}

void cwOpenGLSettings::setNativeTextRendering(bool useNativeTextRendering) {
    if(NativeTextRendering != useNativeTextRendering) {
        Q_ASSERT(thread() == QThread::currentThread());
        NativeTextRendering = useNativeTextRendering;
        updateSettingsWithDevice(nativeTextRenderingKey(), NativeTextRendering);
        setNeedsRestart();
        emit useNativeTextRenderingChanged();
    }
}

void cwOpenGLSettings::setRendererType(cwOpenGLSettings::Renderer rendererType) {
    if(RendererType != rendererType && supportedRenders().contains(rendererType)) {
        Q_ASSERT(thread() == QThread::currentThread());
        RendererType = rendererType;
        updateSettings(rendererKey(), RendererType);
        setNeedsRestart();
        emit rendererTypeChanged();
    }
}

void cwOpenGLSettings::setUseAnisotropy(bool useAnisotropy) {
    if(UseAnisotropy != useAnisotropy) {
        Q_ASSERT(thread() == QThread::currentThread());
        UseAnisotropy = useAnisotropy;
        updateSettingsWithDevice(useAnisotropyKey(), UseAnisotropy);
        emit useAnisotropyChanged();
    }
}

void cwOpenGLSettings::setMagFilter(MagFilter magFilter) {
    if(mMagFilter != magFilter) {
        Q_ASSERT(thread() == QThread::currentThread());
        mMagFilter = magFilter;
        updateSettingsWithDevice(magFilterKey(), mMagFilter);
        emit magFilterChanged();
    }
}

void cwOpenGLSettings::setMinFilter(MinFilter minFilter) {
    if(mMinFilter != minFilter) {
        Q_ASSERT(thread() == QThread::currentThread());
        mMinFilter = minFilter;
        updateSettingsWithDevice(minFilterKey(), mMinFilter);
        emit minFilterChanged();
    }
}

QVector<cwOpenGLSettings::Renderer> cwOpenGLSettings::supportedRenders() const
{
#ifdef Q_OS_WIN
    return {Auto,
                GPU,
                Angles,
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
        TesterSettings tester; //This is used to test if the settings crashes on initialization

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

            Singleton->DXT1Supported = context->extensions().contains("GL_EXT_texture_compression_s3tc_srgb")
                    || context->extensions().contains("GL_EXT_texture_compression_s3tc");
            Singleton->AnisotropySupported = context->extensions().contains("GL_EXT_texture_filter_anisotropic");

            if(Singleton->DXT1Supported) {
                Singleton->GPUGeneratedDXT1Supported = testGPU_DXT1();
            }

            cwOpenGLSettings defaultSettings;

            auto minFilterDefault = [&defaultSettings]() {
                if(Singleton->Vendor.contains("ANGLE")) {
                    return MinLinear;
                }
                return defaultSettings.MinNearest_Mipmap_Linear;
            };

            //Bind the bool* away, and just have nullptr
            auto toInt = std::bind(&QVariant::toInt, std::placeholders::_1, nullptr);

            Singleton->load(Singleton->RendererType,
                            defaultSettings.RendererType,
                            rendererKey(),
                            &cwOpenGLSettings::rootKey,
                            toInt);

            Singleton->load(Singleton->DXT1Compression,
                            defaultSettings.DXT1Compression,
                            dXT1CompressionKey(),
                            &cwOpenGLSettings::keyWithDevice,
                            &QVariant::toBool);

            Singleton->load(Singleton->Mipmaps,
                            defaultSettings.Mipmaps,
                            mipmapsKey(),
                            &cwOpenGLSettings::keyWithDevice,
                            &QVariant::toBool);

            Singleton->load(Singleton->NativeTextRendering,
                            defaultSettings.NativeTextRendering,
                            nativeTextRenderingKey(),
                            &cwOpenGLSettings::keyWithDevice,
                            &QVariant::toBool);

            Singleton->load(Singleton->UseAnisotropy,
                            defaultSettings.UseAnisotropy,
                            useAnisotropyKey(),
                            &cwOpenGLSettings::keyWithDevice,
                            &QVariant::toBool);

            Singleton->load(Singleton->mMagFilter,
                            defaultSettings.mMagFilter,
                            magFilterKey(),
                            &cwOpenGLSettings::keyWithDevice,
                            toInt);

            Singleton->load(Singleton->mMinFilter,
                            minFilterDefault(),
                            minFilterKey(),
                            &cwOpenGLSettings::keyWithDevice,
                            toInt);

            Singleton->load(Singleton->mDXT1Algorithm,
                            Singleton->GPUGeneratedDXT1Supported ? DXT1_GPU : DXT1_Squish,
                            dXT1GenerateAlgroKey(),
                            &cwOpenGLSettings::keyWithDevice,
                            toInt);      

        } else {
            Singleton->Version = "Unknown, couldn't create context";
        }
    }
}

cwOpenGLSettings *cwOpenGLSettings::instance()
{
    return Singleton;
}

void cwOpenGLSettings::cleanup()
{
    delete Singleton;
    Singleton = nullptr;
}

cwOpenGLSettings::Renderer cwOpenGLSettings::perviousRenderer()
{
    QSettings settings;
    if(TesterSettings::crashedOnTest()) {
        updateSettings(rendererKey(), Software);
    }

    cwOpenGLSettings glSettings;
    glSettings.load(glSettings.RendererType,
                    glSettings.RendererType,
                    cwOpenGLSettings::rendererKey(),
                    &cwOpenGLSettings::rootKey,
                    std::bind(&QVariant::toInt, std::placeholders::_1, nullptr));

    return glSettings.RendererType;
}

void cwOpenGLSettings::setToDefault()
{
    cwOpenGLSettings defaultSettings;
    setMipmaps(defaultSettings.useMipmaps());
    setMagFilter(defaultSettings.magFilter());
    setMinFilter(defaultSettings.minFilter());
    setRendererType(defaultSettings.rendererType());
    setDXT1Algorithm(defaultSettings.dxt1Algorithm());
    setUseAnisotropy(defaultSettings.useAnisotropy());
    setUseDXT1Compression(defaultSettings.useDXT1Compression());
    setNativeTextRendering(defaultSettings.useNativeTextRendering());
}

QString cwOpenGLSettings::rootKey(const QString &subKey) const
{
    return key(subKey);
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

    auto toDivisibleBy4 = [](int value)->int {
        return static_cast<int>(std::ceil(value / 4.0) * 4);
    };

    auto sizeTo4 = [toDivisibleBy4](const QSize& size) {
        return QSize(toDivisibleBy4(size.width()), toDivisibleBy4(size.height()));
    };

    QSize size = sizeTo4(openGLResult.size);

    QVector<unsigned int> openGLImage(size.width() * size.height(), 0);
    QVector<unsigned int> squishImage(size.width() * size.height(), 0);

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

    auto diff = sqrt(sumOfSquares / static_cast<double>(size.height() * size.width()));

    return diff < 5.0;
}

void cwOpenGLSettings::setNeedsRestart()
{
    NeedsRestart = true;
    emit needsRestartChanged();
}

void cwOpenGLSettings::updateSettingsWithDevice(const QString &key, const QVariant &value)
{
    QSettings settings;
    settings.setValue(keyWithDevice(key), value);
}

void cwOpenGLSettings::updateSettings(const QString &key, const QVariant &value)
{
    QSettings settings;
    settings.setValue(cwOpenGLSettings::key(key), value);
}

QString cwOpenGLSettings::key(const QString& subKey)
{
    return baseKey().arg(subKey);
}

QString cwOpenGLSettings::keyWithDevice(const QString &subKey) const
{
    auto deviceKey = renderer().remove(QRegularExpression("\\s+|/|-|\\."));
    return QString(baseKey() + "-%2").arg(deviceKey).arg(subKey);
}

void cwOpenGLSettings::setApplicationRenderer()
{
    switch (perviousRenderer()) {

    case Auto:
        break;
    case Angles:
        QCoreApplication::setAttribute(Qt::AA_UseOpenGLES);
        break;
    case GPU:
        QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
        break;
    case Software:
        QCoreApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);
        break;
    }
}

void cwOpenGLSettings::setDXT1Algorithm(DXT1Algorithm dxt1Algorithm) {
    if(mDXT1Algorithm != dxt1Algorithm) {
        Q_ASSERT(thread() == QThread::currentThread());
        mDXT1Algorithm = dxt1Algorithm;
        updateSettingsWithDevice(dXT1GenerateAlgroKey(), mDXT1Algorithm);
        emit dxt1AlgorithmChanged();
    }
}

cwOpenGLSettings::Renderer cwOpenGLSettings::rendererType() const {
    return RendererType;
}

QStringList cwOpenGLSettings::magFilterModel() const {
    return {
        "Nearest",
        "Linear"
    };
}

QStringList cwOpenGLSettings::minFilterModel() const {
    return {
        "Linear",
        "Nearest Mipmap Linear",
        "Linear Mipmap Linear"
    };
}

QStringList cwOpenGLSettings::rendererModel() const {
    auto toString = [](Renderer type)->QString {
        switch(type) {
        case Auto:
            return "Automatic";
        case GPU:
            return "OpenGL";
        case Angles:
            return "OpenGL ES via DirectX";
        case Software:
            return "Software";
        }
        return "";
    };

    auto supported = supportedRenders();
    QStringList renderers;
    renderers.reserve(supported.size());
    std::transform(supported.begin(), supported.end(), std::back_inserter(renderers), toString);
    return renderers;
}

int cwOpenGLSettings::currentSupportedRenderer() const {
    return supportedRenders().indexOf(rendererType());
}

void cwOpenGLSettings::setCurrentSupportedRender(int currentSupportedRenderer) {
    if(this->currentSupportedRenderer() != currentSupportedRenderer) {
        auto supported = supportedRenders();
        auto type = supported.at(currentSupportedRenderer);
        setRendererType(type);
        emit currentSupportedRendererChanged();
    }
}

QString cwOpenGLSettings::allVersionInfo() const {
    QStringList values = {
        QString("Version: %1").arg(version()),
        QString("Shader Version: %2").arg(shadingVersion()),
        QString("Vendor: %1").arg(vendor()),
        QString("Renderer: %1").arg(renderer()),
        QString("Threaded Rendering: %1").arg(QOpenGLContext::supportsThreadedOpenGL() ? "Yes" : "No"),
        QString("Extensions:")
    };

    values.append(extentions());
    return values.join("\n");
}

