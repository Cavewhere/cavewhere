#ifndef CWMANUALSEARCHMODEL_H
#define CWMANUALSEARCHMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QQmlEngine>
#include <QString>
#include <QList>

//Our includes
#include "CaveWhereLibExport.h"

class cwManualIndex;

/**
 * @brief A searchable, chapter-ordered view of the embedded manual for the Docs sidebar.
 *
 * With an empty query the model lists every article in cwManualIndex order (which
 * already groups by chapter), so a ListView can render the table of contents with
 * a section header per chapter. A non-empty query keeps only the articles whose
 * search haystack (title + summary + keywords, from cwManualIndex::searchText)
 * contains every whitespace-separated term, preserving that order. Selecting a row
 * gives QML the slug to navigate to.
 */
class CAVEWHERE_LIB_EXPORT cwManualSearchModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ManualSearchModel)
    Q_PROPERTY(cwManualIndex* manualIndex READ manualIndex WRITE setManualIndex NOTIFY manualIndexChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        SlugRole = Qt::UserRole + 1,
        TitleRole,
        ChapterRole,
        SummaryRole
    };
    Q_ENUM(Roles)

    explicit cwManualSearchModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    cwManualIndex* manualIndex() const { return m_manualIndex; }
    void setManualIndex(cwManualIndex* manualIndex);

    QString query() const { return m_query; }
    void setQuery(const QString& query);

signals:
    void manualIndexChanged();
    void queryChanged();
    void countChanged();

private:
    void rebuild();

    struct Row {
        QString slug;
        QString title;
        QString chapter;
        QString summary;
    };

    cwManualIndex* m_manualIndex = nullptr;
    QString m_query;
    QList<Row> m_rows;  //!< the currently visible (filtered) rows, in chapter order
};

#endif // CWMANUALSEARCHMODEL_H
