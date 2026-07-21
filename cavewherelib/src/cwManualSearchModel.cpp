//Our includes
#include "cwManualSearchModel.h"
#include "cwManualIndex.h"

cwManualSearchModel::cwManualSearchModel(QObject* parent) :
    QAbstractListModel(parent)
{
}

int cwManualSearchModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_rows.size());
}

QVariant cwManualSearchModel::data(const QModelIndex& index, int role) const
{
    if (index.row() < 0 || index.row() >= m_rows.size()) {
        return QVariant();
    }

    const Row& row = m_rows.at(index.row());
    switch (role) {
    case SlugRole:
        return row.slug;
    case TitleRole:
        return row.title;
    case ChapterRole:
        return row.chapter;
    case SummaryRole:
        return row.summary;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> cwManualSearchModel::roleNames() const
{
    return {
        {SlugRole, "slug"},
        {TitleRole, "title"},
        {ChapterRole, "chapter"},
        {SummaryRole, "summary"}
    };
}

void cwManualSearchModel::setManualIndex(cwManualIndex* manualIndex)
{
    if (m_manualIndex == manualIndex) {
        return;
    }
    m_manualIndex = manualIndex;
    rebuild();
    emit manualIndexChanged();
}

void cwManualSearchModel::setQuery(const QString& query)
{
    if (m_query == query) {
        return;
    }
    m_query = query;
    rebuild();
    emit queryChanged();
}

void cwManualSearchModel::rebuild()
{
    beginResetModel();
    m_rows.clear();

    if (m_manualIndex != nullptr) {
        //An empty query lists every article; otherwise keep only those whose
        //haystack contains every whitespace-separated term (AND semantics).
        const QStringList terms = m_query.toLower().split(QChar::Space, Qt::SkipEmptyParts);

        for (const cwManualIndex::Article& article : m_manualIndex->articleList()) {
            bool matches = true;
            if (!terms.isEmpty()) {
                const QString haystack = m_manualIndex->searchText(article.slug);
                for (const QString& term : terms) {
                    if (!haystack.contains(term)) {
                        matches = false;
                        break;
                    }
                }
            }

            if (matches) {
                m_rows.append(Row{article.slug, article.title,
                                  article.chapter, article.summary});
            }
        }
    }

    endResetModel();
    emit countChanged();
}
