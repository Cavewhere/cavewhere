#include "cwRemoteAccountModel.h"

#include <QSettings>

namespace {
static const char AccountsGroup[] = "RemoteAccounts";
}

cwRemoteAccountModel::cwRemoteAccountModel(QObject* parent)
    : QAbstractListModel(parent)
{
    loadFromSettings();
}

int cwRemoteAccountModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_entries.size();
}

QVariant cwRemoteAccountModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }

    const Entry& entry = m_entries.at(index.row());
    switch (role) {
    case ProviderRole:
        return static_cast<int>(entry.provider);
    case UsernameRole:
        return entry.username;
    case LabelRole:
        return providerDisplayName(entry.provider) + QStringLiteral(" (") + entry.username + QStringLiteral(")");
    default:
        return {};
    }
}

QHash<int, QByteArray> cwRemoteAccountModel::roleNames() const
{
    return {
        { ProviderRole, "provider" },
        { UsernameRole, "username" },
        { LabelRole, "label" }
    };
}

void cwRemoteAccountModel::addOrUpdateAccount(Provider provider, const QString& username)
{
    if (username.isEmpty()) {
        return;
    }

    const int existingIndex = findAccount(provider, username);
    if (existingIndex >= 0) {
        return;
    }

    beginInsertRows(QModelIndex(), m_entries.count(), m_entries.count());
    m_entries.append({ provider, username });
    endInsertRows();
    saveToSettings();
    emit accountsChanged();
}

void cwRemoteAccountModel::removeAccount(Provider provider, const QString& username)
{
    const int idx = findAccount(provider, username);
    if (idx < 0) {
        return;
    }

    beginRemoveRows(QModelIndex(), idx, idx);
    m_entries.removeAt(idx);
    endRemoveRows();
    saveToSettings();
    emit accountsChanged();
}

void cwRemoteAccountModel::clear()
{
    if (m_entries.isEmpty()) {
        return;
    }
    beginResetModel();
    m_entries.clear();
    endResetModel();
    saveToSettings();
    emit accountsChanged();
}

QString cwRemoteAccountModel::providerToString(Provider provider)
{
    switch (provider) {
    case Provider::GitHub:
        return QStringLiteral("github");
    case Provider::Bitbucket:
        return QStringLiteral("bitbucket");
    case Provider::GitLab:
        return QStringLiteral("gitlab");
    default:
        return QStringLiteral("unknown");
    }
}

cwRemoteAccountModel::Provider cwRemoteAccountModel::providerFromString(const QString& str)
{
    if (str.compare(QStringLiteral("github"), Qt::CaseInsensitive) == 0) {
        return Provider::GitHub;
    }
    if (str.compare(QStringLiteral("bitbucket"), Qt::CaseInsensitive) == 0) {
        return Provider::Bitbucket;
    }
    if (str.compare(QStringLiteral("gitlab"), Qt::CaseInsensitive) == 0) {
        return Provider::GitLab;
    }
    return Provider::Unknown;
}

QString cwRemoteAccountModel::providerDisplayName(Provider provider)
{
    switch (provider) {
    case Provider::GitHub:
        return QStringLiteral("GitHub");
    case Provider::Bitbucket:
        return QStringLiteral("Bitbucket");
    case Provider::GitLab:
        return QStringLiteral("GitLab");
    default:
        return QStringLiteral("Unknown");
    }
}

int cwRemoteAccountModel::findAccount(Provider provider, const QString& username) const
{
    for (int i = 0; i < m_entries.count(); ++i) {
        const Entry& entry = m_entries.at(i);
        if (entry.provider == provider && entry.username.compare(username, Qt::CaseInsensitive) == 0) {
            return i;
        }
    }
    return -1;
}

void cwRemoteAccountModel::loadFromSettings()
{
    m_entries.clear();
    QSettings settings;
    settings.beginGroup(AccountsGroup);
    const int size = settings.beginReadArray("accounts");
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        const auto provider = providerFromString(settings.value("provider").toString());
        const QString username = settings.value("username").toString();
        if (!username.isEmpty()) {
            m_entries.append({ provider, username });
        }
    }
    settings.endArray();
    settings.endGroup();
}

void cwRemoteAccountModel::saveToSettings() const
{
    QSettings settings;
    settings.beginGroup(AccountsGroup);
    settings.beginWriteArray("accounts");
    for (int i = 0; i < m_entries.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("provider", providerToString(m_entries.at(i).provider));
        settings.setValue("username", m_entries.at(i).username);
    }
    settings.endArray();
    settings.endGroup();
}
