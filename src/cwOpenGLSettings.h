#ifndef CWOPENGLSETTINGS_H
#define CWOPENGLSETTINGS_H

//Qt includes
#include <QObject>
#include <QSettings>
#include <QDebug>

//Our includes
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwOpenGLSettings : public QObject
{
    Q_OBJECT

    //For texture maps
    Q_PROPERTY(bool dxt1Supported READ dxt1Supported CONSTANT)
    Q_PROPERTY(bool anisotropySupported READ anisotropySupported CONSTANT)
    Q_PROPERTY(bool useDXT1Compression READ useDXT1Compression WRITE setUseDXT1Compression NOTIFY useDXT1CompressionChanged)
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

    //For QML
    Q_PROPERTY(QStringList magFilterModel READ magFilterModel CONSTANT)
    Q_PROPERTY(QStringList minFilterModel READ minFilterModel CONSTANT)
    Q_PROPERTY(QStringList rendererModel READ rendererModel CONSTANT)
    Q_PROPERTY(int currentSupportedRenderer READ currentSupportedRenderer WRITE setCurrentSupportedRender NOTIFY currentSupportedRendererChanged)

    //OpenGL information
    Q_PROPERTY(QString vendor READ vendor CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString renderer READ renderer CONSTANT)
    Q_PROPERTY(QString shadingVersion READ shadingVersion CONSTANT)
    Q_PROPERTY(QStringList extentions READ extentions CONSTANT)
    Q_PROPERTY(QString allVersionInfo READ allVersionInfo CONSTANT)

    Q_PROPERTY(bool needsRestart READ needsRestart NOTIFY needsRestartChanged)

public:
    //Don't change order because this will mess-up QSettings
    enum Renderer {
        Auto,
        GPU,
        Angles,
        Software
    };

    //Don't change order because this will mess-up QSettings
    enum MagFilter {
        MagNearest,
        MagLinear
    };

    //Don't change order because this will mess-up QSettings
    enum MinFilter {
        MinLinear,
        MinNearest_Mipmap_Linear,
        MinLinear_Mipmap_Linear
    };

    //Don't change order because this will mess-up QSettings
    enum DXT1Algorithm {
        DXT1_GPU,
        DXT1_Squish
    };

    Q_ENUM(Renderer)
    Q_ENUM(MagFilter)
    Q_ENUM(MinFilter)
    Q_ENUM(DXT1Algorithm)

    cwOpenGLSettings(const cwOpenGLSettings& other) = delete;

    bool dxt1Supported() const;
    bool anisotropySupported() const;

    bool useDXT1Compression() const;
    void setUseDXT1Compression(bool useDXT1Compression);

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

    int currentSupportedRenderer() const;
    void setCurrentSupportedRender(int currentSupportedRenderer);

    bool needsRestart() const;

    QStringList magFilterModel() const;
    QStringList minFilterModel() const;
    QStringList rendererModel() const;

    QString vendor() const;
    QString version() const;
    QString renderer() const;
    QString shadingVersion() const;
    QStringList extentions() const;
    QString allVersionInfo() const;

    QVector<Renderer> supportedRenders() const;

    static void initialize();
    static cwOpenGLSettings* instance();
    static void cleanup();

    static Renderer perviousRenderer();

    void setToDefault();

    static QString key(const QString& subKey);
    QString rootKey(const QString &subKey) const;
    QString keyWithDevice(const QString& subKey) const;

    static void setApplicationRenderer();


signals:
    void useDXT1CompressionChanged();
    void useMipmapsChanged();
    void useNativeTextRenderingChanged();
    void rendererTypeChanged();
    void currentSupportedRendererChanged();
    void needsRestartChanged();
    void useAnisotropyChanged();
    void magFilterChanged();
    void minFilterChanged();
    void dxt1AlgorithmChanged();

private:
    class TesterSettings {
    public:
        TesterSettings() {
            setSettings(true);
        }

        ~TesterSettings() {
            setSettings(false);
        }

        static bool crashedOnTest() {
            QSettings settings;
            return settings.value(cwOpenGLSettings::key(cwOpenGLSettings::TestingKey), false).toBool();
        }

    private:
        void setSettings(bool value) {
            QSettings settings;
            settings.setValue(cwOpenGLSettings::key(cwOpenGLSettings::TestingKey), value);
        }
    };

    cwOpenGLSettings(QObject* parent = nullptr);

    //Texture settings
    bool DXT1Supported = false; //!<
    bool AnisotropySupported = false; //!<
    bool DXT1Compression = true; //!<
    bool UseAnisotropy = true; //!<
    bool Mipmaps = true; //!<
    bool GPUGeneratedDXT1Supported = true; //!<
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

    static const QString BaseKey;
    static const QString TestingKey;
    static const QString RendererKey;
    static const QString DXT1CompressionKey;
    static const QString MipmapsKey;
    static const QString NativeTextRenderingKey;
    static const QString UseAnisotropyKey;
    static const QString MagFilterKey;
    static const QString MinFilterKey;
    static const QString DXT1GenerateAlgroKey;

    static cwOpenGLSettings* Singleton; //This singlton isn't threadsafe

    static bool testGPU_DXT1();

    void setNeedsRestart();
    void updateSettingsWithDevice(const QString& key, const QVariant& value);
    static void updateSettings(const QString& key, const QVariant& value);

    template<typename T, typename KeyFunc, typename Func>
    void load(T& value, T defaultValue, const QString& keyName, KeyFunc keyFunc, Func variantFunc) {
        QSettings setting;
        auto key = std::invoke(keyFunc, this, keyName);
        QVariant settingsValue = setting.value(key, defaultValue);
        value = static_cast<T>(std::invoke(variantFunc, settingsValue));
    }
};

Q_DECLARE_METATYPE(QVector<cwOpenGLSettings::Renderer>)


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
    return dxt1Supported() && GPUGeneratedDXT1Supported;
}

inline cwOpenGLSettings::DXT1Algorithm cwOpenGLSettings::dxt1Algorithm() const {
    return mDXT1Algorithm;
}

#endif // CWOPENGLSETTINGS_H
