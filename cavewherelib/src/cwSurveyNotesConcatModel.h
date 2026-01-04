#pragma once

/**************************************************************************
**
**    Copyright (C) 2025 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Qt
#include <QConcatenateTablesProxyModel>
#include <QPointer>
#include <QUrl>
#include <QQmlEngine>

// Fwd decls
class cwTrip;
class cwSurveyNoteModel;
class cwSurveyNoteLiDARModel;

//Our includes
#include "cwGlobals.h"

/**
 * @brief Concatenates cwTrip's image/PDF notes and LiDAR notes into one model.
 *
 * Set a valid cwTrip via the trip property to attach:
 *   - trip->notes()       (cwSurveyNoteModel)
 *   - trip->notesLiDAR()  (cwSurveyNoteLiDARModel)
 *
 * addFiles(...) will split inputs and forward images/PDFs to notes(),
 * and .glb to notesLiDAR().
 */
class CAVEWHERE_LIB_EXPORT cwSurveyNotesConcatModel : public QConcatenateTablesProxyModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SurveyNotesConcatModel)

    Q_PROPERTY(cwTrip* trip READ trip WRITE setTrip NOTIFY tripChanged)

public:
    explicit cwSurveyNotesConcatModel(QObject* parent = nullptr);
    ~cwSurveyNotesConcatModel() override = default;

    QHash<int, QByteArray> roleNames() const override;

    cwTrip* trip() const;
    void setTrip(cwTrip* trip);

    Q_INVOKABLE void addFiles(QList<QUrl> files);
    Q_INVOKABLE void removeNote(int index);

signals:
    void tripChanged(cwTrip* trip);

private:
    void refreshSources();
    void clearSources();

    static bool isPdfPath(const QString& absolutePath);
    static bool isGlbUrl(const QUrl& fileUrl);
    static bool isImageLikeUrl(const QUrl& fileUrl);

private: // members last
    QPointer<cwTrip> m_trip;
    QPointer<cwSurveyNoteModel> m_notesModel;
    QPointer<cwSurveyNoteLiDARModel> m_notesLiDARModel;

};
