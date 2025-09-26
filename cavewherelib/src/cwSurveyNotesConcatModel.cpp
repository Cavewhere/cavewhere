#include "cwSurveyNotesConcatModel.h"

// CaveWhere
#include "cwTrip.h"
#include "cwSurveyNoteModel.h"
#include "cwSurveyNoteLiDARModel.h"
#include "cwPDFConverter.h"

// Qt
#include <QFileInfo>
#include <QImageReader>
#include <QSet>
#include <algorithm>

cwSurveyNotesConcatModel::cwSurveyNotesConcatModel(QObject* parent)
    : QConcatenateTablesProxyModel(parent)
{
}

QHash<int, QByteArray> cwSurveyNotesConcatModel::roleNames() const
{
    if(m_notesModel) {
        return m_notesModel->roleNames();
    }
    return {};
}

cwTrip* cwSurveyNotesConcatModel::trip() const
{
    return m_trip;
}

void cwSurveyNotesConcatModel::setTrip(cwTrip* trip)
{
    if (m_trip == trip) {
        return;
    }

    // Disconnect previous trip destroy signal (QPointer auto-null is enough, but we also refresh).
    if (m_trip) {
        disconnect(m_trip, nullptr, this, nullptr);
    }

    m_trip = trip;
    emit tripChanged(m_trip);

    // If trip is destroyed, clear sources.
    if (m_trip) {
        connect(m_trip, &QObject::destroyed, this, [this]() {
            m_trip = nullptr;
            clearSources();
        });
    }

    refreshSources();
}

void cwSurveyNotesConcatModel::refreshSources()
{
    clearSources();

    if (m_trip == nullptr) {
        return;
    }

    m_notesModel = m_trip->notes();
    m_notesLiDARModel = m_trip->notesLiDAR();

    if (m_notesModel) {
        addSourceModel(m_notesModel);
    }
    if (m_notesLiDARModel) {
        addSourceModel(m_notesLiDARModel);
    }
}

void cwSurveyNotesConcatModel::clearSources()
{
    if (m_notesModel) {
        removeSourceModel(m_notesModel);
        m_notesModel = nullptr;
    }
    if (m_notesLiDARModel) {
        removeSourceModel(m_notesLiDARModel);
        m_notesLiDARModel = nullptr;
    }
}

// ---------- File routing ----------

bool cwSurveyNotesConcatModel::isPdfPath(const QString& absolutePath)
{
    if (!cwPDFConverter::isSupported()) {
        return false;
    }
    const QFileInfo fileInfo(absolutePath);
    return fileInfo.suffix().compare(QStringLiteral("pdf"), Qt::CaseInsensitive) == 0;
}

bool cwSurveyNotesConcatModel::isGlbUrl(const QUrl& fileUrl)
{
    const QFileInfo fileInfo(fileUrl.toLocalFile());
    return fileInfo.suffix().compare(QStringLiteral("glb"), Qt::CaseInsensitive) == 0;
}

bool cwSurveyNotesConcatModel::isImageLikeUrl(const QUrl& fileUrl)
{
    static const QSet<QString> supportedImageSuffixes = []() {
        QSet<QString> s;
        const auto formats = QImageReader::supportedImageFormats();
        s.reserve(formats.size());
        for (const QByteArray& fmt : formats) {
            s.insert(QString::fromLatin1(fmt).toLower());
        }
        return s;
    }();

    const QFileInfo fileInfo(fileUrl.toLocalFile());
    const QString suffixLower = fileInfo.suffix().toLower();

    if (supportedImageSuffixes.contains(suffixLower)) {
        return true;
    }
    if (isPdfPath(fileInfo.absoluteFilePath())) {
        return true;
    }
    return false;
}

void cwSurveyNotesConcatModel::addFiles(QList<QUrl> files)
{
    if (m_trip == nullptr) {
        return;
    }

    // We can forward directly to models if you prefer,
    // but we split so each model only sees its relevant inputs.

    QList<QUrl> fileUrls = files; // mutable copy

    qDebug() << "FileUrls:" << fileUrls;

    const auto middle = std::partition(fileUrls.begin(), fileUrls.end(),
                                       [this](const QUrl& url) {
                                           // image-like but not GLB
                                           return isImageLikeUrl(url) && !isGlbUrl(url);
                                       });

    const QList<QUrl> imageLikeUrls(fileUrls.begin(), middle);

    QList<QUrl> glbUrls;
    glbUrls.reserve(std::distance(middle, fileUrls.end()));
    for (auto it = middle; it != fileUrls.end(); ++it) {
        if (isGlbUrl(*it)) {
            glbUrls.append(*it);
        }
    }

    qDebug() << "Images:" << imageLikeUrls;
    qDebug() << "Glb:" << glbUrls;

    if (!imageLikeUrls.isEmpty() && m_trip->notes() != nullptr) {
        m_trip->notes()->addFromFiles(imageLikeUrls);
    }
    if (!glbUrls.isEmpty() && m_trip->notesLiDAR() != nullptr) {
        m_trip->notesLiDAR()->addFromFiles(glbUrls);
    }
}
