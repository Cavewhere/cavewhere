#ifndef CWOPENGLSETTINGS_H
#define CWOPENGLSETTINGS_H

//Qt includes
#include <QObject>

//Our includes
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwOpenGLSettings : public QObject
{
    Q_OBJECT

    //For texture maps
    Q_PROPERTY(bool dxt1Supported READ dxt1Supported CONSTANT)
    Q_PROPERTY(bool anisotropySupported READ anisotropySupported CONSTANT)
    Q_PROPERTY(bool useDXT1Compression READ useDXT1Compression WRITE setDXT1Compression NOTIFY useDXT1CompressionChanged)
    Q_PROPERTY(bool useAnisotropy READ useAnisotropy WRITE setUseAnisotropy NOTIFY useAnisotropyChanged)
    Q_PROPERTY(bool useMipmaps READ useMipmaps WRITE setMipmaps NOTIFY useMipmapsChanged)
    Q_PROPERTY(MagFilter magFilter READ magFilter WRITE setMagFilter NOTIFY magFilterChanged)
    Q_PROPERTY(MinFilter minFilter READ minFilter WRITE setMinFilter NOTIFY minFilterChanged)

    //Texture generation
    Q_PROPERTY(bool gpuGeneratedDXT1Supported READ gpuGeneratedDXT1Supported CONSTANT)
    Q_PROPERTY(DXT1Algorithm dxt1Algorithm READ dxt1Algorithm WRITE setDXT1Algorithm NOTIFY dxt1AlgorithmChanged)

    //For text rendering in qml
    Q_PROPERTY(bool useNativeTextRendering READ useNativeTextRendering WRITE setNativeTextRendering NOTIFY useNativeTextRenderingChanged)
    Q_PROPERTY(cwOpenGLSettings::Renderer rendererType READ rendererType WRITE setRendererType NOTIFY rendererTypeChanged)

    //OpenGL information
    Q_PROPERTY(QString vendor READ vendor CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString renderer READ renderer CONSTANT)
    Q_PROPERTY(QString shadingVersion READ shadingVersion CONSTANT)
    Q_PROPERTY(QStringList extentions READ extentions CONSTANT)

    Q_PROPERTY(bool needsRestart READ needsRestart NOTIFY needsRestartChanged)

public:
    enum Renderer {
        Auto,
        Angles,
        Destop,
        Software
    };

    enum MagFilter {
        MagNearest,
        MagLinear
    };

    enum MinFilter {
        MinLinear,
        MinNearest_Mipmap_Linear,
        MinLinear_Mipmap_Linear
    };

    enum DXT1Algorithm {
        DXT1_GPU,
        DXT1_Squish
    };

    Q_ENUM(Renderer)
    Q_ENUM(MagFilter)
    Q_ENUM(MinFilter)
    Q_ENUM(DXT1Algorithm)

    cwOpenGLSettings(QObject* parent = nullptr);

    bool dxt1Supported() const;
    bool anisotropySupported() const;

    bool useDXT1Compression() const;
    void setDXT1Compression(bool useDXT1Compression);

    bool useAnisotropy() const;
    void setUseAnisotropy(bool useAnisotropy);

    bool useMipmaps() const;
    void setMipmaps(bool useMipmaps);

    MagFilter magFilter() const;
    void setMagFilter(MagFilter magFilter);

    MinFilter minFilter() const;
    void setMinFilter(MinFilter minFilter);

    bool gpuGeneratedDXT1Supported() const;

    DXT1Algorithm dxt1Algorithm() const;
    void setDXT1Algorithm(DXT1Algorithm dxt1Algorithm);

    bool useNativeTextRendering() const;
    void setNativeTextRendering(bool useNativeTextRendering);

    cwOpenGLSettings::Renderer rendererType() const;
    void setRendererType(cwOpenGLSettings::Renderer rendererType);

    bool needsRestart() const;

    QString vendor() const;
    QString version() const;
    QString renderer() const;
    QString shadingVersion() const;
    QStringList extentions() const;

    QVector<Renderer> supportedRenders() const;

    static void initialize();
    static cwOpenGLSettings* instance();

signals:
    void useDXT1CompressionChanged();
    void useMipmapsChanged();
    void useNativeTextRenderingChanged();
    void rendererTypeChanged();
    void needsRestartChanged();
    void useAnisotropyChanged();
    void magFilterChanged();
    void minFilterChanged();
    void dxt1AlgorithmChanged();

private:
    //Texture settings
    bool DXT1Supported = false; //!<
    bool AnisotropySupported = false; //!<
    bool DXT1Compression = true; //!<
    bool UseAnisotropy = true; //!<
    bool Mipmaps = true; //!<
    bool GPUGeneratedDXT1Supported = false; //!<
    DXT1Algorithm mDXT1Algorithm = DXT1_Squish; //!<
    MagFilter mMagFilter = MagNearest; //!<
    MinFilter mMinFilter = MinNearest_Mipmap_Linear; //!<

    //Texture rendering
    bool NativeTextRendering = false; //!<

    //OpenGL info
    cwOpenGLSettings::Renderer RendererType = Auto; //!<
    QString Version; //!<
    QString Vendor; //!<
    QString ShadingVersion; //!<
    QString OpenGLRenderer; //!<
    QStringList Extentions; //!<

    bool NeedsRestart = false; //!<

    static cwOpenGLSettings* Singleton; //This singlton isn't threadsafe
};


inline bool cwOpenGLSettings::useMipmaps() const {
    return Mipmaps;
}

inline bool cwOpenGLSettings::dxt1Supported() const {
    return DXT1Supported;
}

inline bool cwOpenGLSettings::useNativeTextRendering() const {
    return NativeTextRendering;
}

inline bool cwOpenGLSettings::useDXT1Compression() const {
    return DXT1Compression && dxt1Supported();
}

inline bool cwOpenGLSettings::useAnisotropy() const {
    return UseAnisotropy && anisotropySupported();
}

inline cwOpenGLSettings::Renderer cwOpenGLSettings::rendererType() const {
    return RendererType;
}

inline bool cwOpenGLSettings::anisotropySupported() const {
    return AnisotropySupported;
}

inline bool cwOpenGLSettings::needsRestart() const {
    return NeedsRestart;
}

inline QString cwOpenGLSettings::vendor() const {
    return Vendor;
}

inline QString cwOpenGLSettings::version() const {
    return Version;
}

inline QString cwOpenGLSettings::shadingVersion() const {
    return ShadingVersion;
}

inline QString cwOpenGLSettings::renderer() const {
    return OpenGLRenderer;
}

inline QStringList cwOpenGLSettings::extentions() const {
    return Extentions;
}

inline cwOpenGLSettings::MagFilter cwOpenGLSettings::magFilter() const {
    return mMagFilter;
}

inline cwOpenGLSettings::MinFilter cwOpenGLSettings::minFilter() const {
    return mMinFilter;
}

inline bool cwOpenGLSettings::gpuGeneratedDXT1Supported() const {
    return GPUGeneratedDXT1Supported;
}

inline cwOpenGLSettings::DXT1Algorithm cwOpenGLSettings::dxt1Algorithm() const {
    return mDXT1Algorithm;
}

#endif // CWOPENGLSETTINGS_H
