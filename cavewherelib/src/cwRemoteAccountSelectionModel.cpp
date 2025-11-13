#include "cwRemoteAccountSelectionModel.h"

#include "cwRemoteAccountModel.h"

cwRemoteAccountSelectionModel::cwRemoteAccountSelectionModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int cwRemoteAccountSelectionModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid() || !m_baseModel) {
        return 0;
    }
    int total = 1; // None entry
    if (showAccountSeparator()) {
        total += 1; // separator
    }
    total += m_baseModel->rowCount();
    if (showAddSeparator()) {
        total += 1; // separator before Add
    }
    total += 1; // Add entry
    return total;
}

QVariant cwRemoteAccountSelectionModel::data(const QModelIndex& index, int role) const
{
    if (!m_baseModel || !index.isValid()) {
        return {};
    }

    const int total = rowCount();
    if (index.row() < 0 || index.row() >= total) {
        return {};
    }

    const int accountCount = m_baseModel->rowCount();
    const bool hasSeparatorBeforeAccounts = showAccountSeparator();
    const bool hasSeparatorBeforeAdd = showAddSeparator();

    if (index.row() == 0) {
        if (role == LabelRole) {
            return QStringLiteral("None");
        } else if (role == ProviderRole) {
            return static_cast<int>(cwRemoteAccountModel::Provider::Unknown);
        } else if (role == UsernameRole) {
            return QString();
        } else if (role == EntryTypeRole) {
            return NoneEntry;
        }
        return {};
    }

    int currentRow = 1;

    if (hasSeparatorBeforeAccounts) {
        if (index.row() == currentRow) {
            return role == EntryTypeRole ? SeparatorEntry : QVariant();
        }
        ++currentRow;
    }

    if (accountCount > 0 && index.row() >= currentRow && index.row() < currentRow + accountCount) {
        const int baseIndex = index.row() - currentRow;
        auto qIdx = m_baseModel->index(baseIndex, 0);
        switch (role) {
        case LabelRole:
            return m_baseModel->data(qIdx, cwRemoteAccountModel::LabelRole);
        case ProviderRole:
            return m_baseModel->data(qIdx, cwRemoteAccountModel::ProviderRole);
        case UsernameRole:
            return m_baseModel->data(qIdx, cwRemoteAccountModel::UsernameRole);
        case EntryTypeRole:
            return AccountEntry;
        default:
            return {};
        }
    }

    currentRow += accountCount;

    if (hasSeparatorBeforeAdd) {
        if (index.row() == currentRow) {
            return role == EntryTypeRole ? SeparatorEntry : QVariant();
        }
        ++currentRow;
    }

    if (index.row() == currentRow) {
        if (role == LabelRole) {
            return QStringLiteral("Add Account");
        } else if (role == ProviderRole) {
            return static_cast<int>(cwRemoteAccountModel::Provider::Unknown);
        } else if (role == UsernameRole) {
            return QString();
        } else if (role == EntryTypeRole) {
            return AddEntry;
        }
        return {};
    }

    return {};
}

QHash<int, QByteArray> cwRemoteAccountSelectionModel::roleNames() const
{
    return {
        { LabelRole, "label" },
        { ProviderRole, "provider" },
        { UsernameRole, "username" },
        { EntryTypeRole, "entryType" }
    };
}

void cwRemoteAccountSelectionModel::setSourceModel(cwRemoteAccountModel* model)
{
    if (m_baseModel == model) {
        return;
    }

    beginResetModel();
    if (m_baseModel) {
        disconnect(m_baseModel, nullptr, this, nullptr);
    }
    m_baseModel = model;
    if (m_baseModel) {
        auto rebuild = [this]() {
            beginResetModel();
            endResetModel();
            emit entriesChanged();
        };
        connect(m_baseModel, &cwRemoteAccountModel::modelReset, this, rebuild);
        connect(m_baseModel, &cwRemoteAccountModel::rowsInserted, this, rebuild);
        connect(m_baseModel, &cwRemoteAccountModel::rowsRemoved, this, rebuild);
        connect(m_baseModel, &cwRemoteAccountModel::dataChanged, this, rebuild);
    }
    endResetModel();
    emit entriesChanged();
    emit sourceModelChanged();
}

bool cwRemoteAccountSelectionModel::showAccountSeparator() const
{
    return m_baseModel && m_baseModel->rowCount() > 0;
}

bool cwRemoteAccountSelectionModel::showAddSeparator() const
{
    return true; //m_baseModel && m_baseModel->rowCount() > 0;
}
