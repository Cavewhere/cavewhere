#ifndef CWIMAGE_H
#define CWIMAGE_H

#include "cwGlobals.h"

// Qt includes
#include <QString>
#include <QImage>
#include <QStringList>
#include <QMetaType>
#include <QSharedData>
#include <QSharedDataPointer>
#include <QDebug>
#include <QQmlEngine>
#include <variant>

/**
  \brief This class stores image references either by legacy ID or file path.

  It supports both ID-based (original, mipmaps, icon) and path-based usage,
  with a shared interface backed by a variant-based data structure.
  */
class CAVEWHERE_LIB_EXPORT cwImage {
    Q_GADGET
    QML_VALUE_TYPE(cwImage)

    Q_PROPERTY(QSize originalSize READ originalSize WRITE setOriginalSize)
    Q_PROPERTY(int originalDotsPerMeter READ originalDotsPerMeter WRITE setOriginalDotsPerMeter)
    Q_PROPERTY(QString path READ path WRITE setPath)
    Q_PROPERTY(int page READ page WRITE setPage)
    Q_PROPERTY(Unit unit READ unit WRITE setUnit)

public:
    enum class Unit {
        Pixels = 0,
        Points = 1,
        SvgUnits = 2
    };
    Q_ENUM(Unit)

    enum class Mode {
        Invalid,
        Ids,
        Path
    };
    Q_ENUM(Mode)

    cwImage() : m_data(new SharedData) {}
    virtual ~cwImage() = default;

    void setMipmaps(QList<int> mipmapFiles) {
        ensureIdData();
        idData().mipmapIds = std::move(mipmapFiles);
    }

    QList<int> mipmaps() const {
        Q_ASSERT(std::holds_alternative<IdData>(m_data->modeData));
        return std::get<IdData>(m_data->modeData).mipmapIds;
    }

    void setIcon(int icon) {
        ensureIdData();
        idData().iconId = icon;
    }

    int icon() const {
        Q_ASSERT(std::holds_alternative<IdData>(m_data->modeData));
        return std::get<IdData>(m_data->modeData).iconId;
    }

    void setOriginal(int originalId) {
        ensureIdData();
        idData().originalId = originalId;
    }

    int original() const {
        Q_ASSERT(std::holds_alternative<IdData>(m_data->modeData));
        return std::get<IdData>(m_data->modeData).originalId;
    }

    void setOriginalSize(QSize size) {
        m_data->originalSize = std::move(size);
    }

    QSize originalSize() const {
        return m_data->originalSize;
    }

    void setOriginalDotsPerMeter(int dotsPerMeter) {
        m_data->originalDotsPerMeter = dotsPerMeter;
    }

    int originalDotsPerMeter() const {
        return m_data->originalDotsPerMeter;
    }

    bool operator==(const cwImage& other) const;

    bool operator!=(const cwImage& other) const {
        return !(*this == other);
    }

    bool isOriginalValid() const {
        Q_ASSERT(std::holds_alternative<IdData>(m_data->modeData));
        return std::holds_alternative<IdData>(m_data->modeData) && isIdValid(original());
    }

    bool isMipmapsValid() const;

    bool isIconValid() const {
        Q_ASSERT(std::holds_alternative<IdData>(m_data->modeData));
        return std::holds_alternative<IdData>(m_data->modeData) && isIdValid(icon());
    }

    QList<int> ids() const;

    void setPath(const QString& path) {
        ensurePathData();
        pathData().path = path;
    }

    QString path() const {
        Q_ASSERT(std::holds_alternative<PathData>(m_data->modeData));
        return std::get<PathData>(m_data->modeData).path;
    }

    void setPage(int page) {
        m_data->page = page;
    }

    int page() const {
        return m_data->page;
    }

    void setUnit(Unit unit) {
        m_data->unit = unit;
    }

    Unit unit() const {
        return m_data->unit;
    }

    bool isValid() const {
        //Id mode shouldn't be used, and should only be in old loading code
        return (mode() == Mode::Path && !path().isEmpty())
               || (mode() == Mode::Ids && isOriginalValid()); //This is needed for loading old code
    }

    Mode mode() const {
        if (std::holds_alternative<IdData>(m_data->modeData)) { return Mode::Ids; };
        if (std::holds_alternative<PathData>(m_data->modeData)) { return Mode::Path; };
        return Mode::Invalid;
    }

private:
    static bool isIdValid(int id) {
        return id > -1;
    }

    struct IdData {
        int originalId = -1;
        int iconId = -1;
        QList<int> mipmapIds;

        bool operator==(const IdData& other) const {
            return originalId == other.originalId &&
                   iconId == other.iconId &&
                   mipmapIds == other.mipmapIds;
        }
    };

    struct PathData {
        QString path;

        bool operator==(const PathData& other) const {
            return path == other.path;
        }
    };

    struct SharedData : public QSharedData {
        QSize originalSize;
        int originalDotsPerMeter = 0;
        int page = -1;
        Unit unit = Unit::Pixels;
        std::variant<std::monostate, IdData, PathData> modeData;

        SharedData() = default;
        SharedData(const SharedData&) = default;
        ~SharedData() = default;
    };

    QSharedDataPointer<SharedData> m_data;

    IdData& idData() {
        return std::get<IdData>(m_data->modeData);
    }

    const IdData& idData() const {
        return std::get<IdData>(m_data->modeData);
    }

    PathData& pathData() {
        return std::get<PathData>(m_data->modeData);
    }

    const PathData& pathData() const {
        return std::get<PathData>(m_data->modeData);
    }

    void ensureIdData() {
        if (!std::holds_alternative<IdData>(m_data->modeData)) {
            m_data->modeData = IdData();
        }
    }

    void ensurePathData() {
        if (!std::holds_alternative<PathData>(m_data->modeData)) {
            m_data->modeData = PathData();
        }
    }
};

Q_DECLARE_METATYPE(cwImage)
CAVEWHERE_LIB_EXPORT QDebug operator<<(QDebug debug, const cwImage &image);

#endif // CWIMAGE_H
