#include "cwRemoteAccountModel.h"

#include <QSettings>
#include <QSet>
#include <QUuid>

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
        return providerDisplayName(entry.provider) + QStringLiteral(" (")
               + (entry.displayName.isEmpty() ? entry.username : entry.displayName)
               + QStringLiteral(")");
    case AccountIdRole:
        return entry.accountId;
    case AuthStateRole:
        return static_cast<int>(entry.authState);
    case ActiveRole:
        return entry.active;
    case LastUsedAtRole:
        return entry.lastUsedAt;
    default:
        return {};
    }
}

QHash<int, QByteArray> cwRemoteAccountModel::roleNames() const
{
    return {
        { ProviderRole, "provider" },
        { UsernameRole, "username" },
        { LabelRole, "label" },
        { AccountIdRole, "accountId" },
        { AuthStateRole, "authState" },
        { ActiveRole, "active" },
        { LastUsedAtRole, "lastUsedAt" }
    };
}

QString cwRemoteAccountModel::upsertAccount(Provider provider, const QString& username)
{
    const QString normalizedUsername = username.trimmed();
    if (normalizedUsername.isEmpty()) {
        return {};
    }

    const int existingIndex = findAccount(provider, normalizedUsername);
    if (existingIndex >= 0) {
        Entry& entry = m_entries[existingIndex];
        if (entry.username != normalizedUsername) {
            entry.username = normalizedUsername;
            const QModelIndex row = index(existingIndex, 0);
            emit dataChanged(row, row, { UsernameRole, LabelRole });
            saveToSettings();
            emit accountsChanged();
        }
        return entry.accountId;
    }

    beginInsertRows(QModelIndex(), m_entries.count(), m_entries.count());
    Entry entry;
    entry.accountId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.provider = provider;
    entry.username = normalizedUsername;
    m_entries.append(entry);
    endInsertRows();
    saveToSettings();
    emit accountsChanged();
    return m_entries.constLast().accountId;
}

QString cwRemoteAccountModel::accountIdFor(Provider provider, const QString& username) const
{
    const int idx = findAccount(provider, username);
    if (idx < 0) {
        return {};
    }
    return m_entries.at(idx).accountId;
}

void cwRemoteAccountModel::setAuthState(const QString& accountId, AuthState state)
{
    const int idx = findAccountById(accountId);
    if (idx < 0 || m_entries[idx].authState == state) {
        return;
    }

    m_entries[idx].authState = state;
    const QModelIndex row = index(idx, 0);
    emit dataChanged(row, row, { AuthStateRole });
    saveToSettings();
    emit accountsChanged();
}

void cwRemoteAccountModel::setActiveAccount(Provider provider, const QString& accountId)
{
    int targetIndex = -1;
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].provider == provider && m_entries[i].accountId == accountId) {
            targetIndex = i;
            break;
        }
    }
    if (targetIndex < 0) {
        return;
    }
    setAccountActiveByIndex(targetIndex);
}

QString cwRemoteAccountModel::activeAccountId(Provider provider) const
{
    for (const Entry& entry : m_entries) {
        if (entry.provider == provider && entry.active) {
            return entry.accountId;
        }
    }
    return {};
}

cwRemoteAccountInfo cwRemoteAccountModel::accountById(const QString& accountId) const
{
    const int idx = findAccountById(accountId);
    if (idx < 0) {
        return {};
    }

    const Entry& entry = m_entries.at(idx);
    return cwRemoteAccountInfo{
        .accountId = entry.accountId,
        .provider = static_cast<int>(entry.provider),
        .username = entry.username,
        .authState = static_cast<int>(entry.authState),
        .active = entry.active,
        .lastUsedAt = entry.lastUsedAt,
        .displayName = entry.displayName
    };
}

void cwRemoteAccountModel::removeAccountById(const QString& accountId)
{
    const int idx = findAccountById(accountId);
    if (idx < 0) {
        return;
    }

    beginRemoveRows(QModelIndex(), idx, idx);
    m_entries.removeAt(idx);
    endRemoveRows();
    saveToSettings();
    emit accountsChanged();
}

void cwRemoteAccountModel::setLastUsedAt(const QString& accountId, const QDateTime& value)
{
    const int idx = findAccountById(accountId);
    if (idx < 0 || m_entries[idx].lastUsedAt == value) {
        return;
    }

    m_entries[idx].lastUsedAt = value;
    const QModelIndex row = index(idx, 0);
    emit dataChanged(row, row, { LastUsedAtRole });
    saveToSettings();
    emit accountsChanged();
}

void cwRemoteAccountModel::addOrUpdateAccount(Provider provider, const QString& username)
{
    upsertAccount(provider, username);
}

void cwRemoteAccountModel::removeAccount(Provider provider, const QString& username)
{
    const int idx = findAccount(provider, username);
    if (idx < 0) {
        return;
    }
    removeAccountById(m_entries[idx].accountId);
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

QString cwRemoteAccountModel::authStateToString(AuthState state)
{
    switch (state) {
    case AuthState::Authorized:
        return QStringLiteral("authorized");
    case AuthState::SignedOut:
        return QStringLiteral("signed_out");
    case AuthState::Revoked:
        return QStringLiteral("revoked");
    case AuthState::Error:
        return QStringLiteral("error");
    default:
        return QStringLiteral("unknown");
    }
}

cwRemoteAccountModel::AuthState cwRemoteAccountModel::authStateFromString(const QString& str)
{
    if (str.compare(QStringLiteral("authorized"), Qt::CaseInsensitive) == 0) {
        return AuthState::Authorized;
    }
    if (str.compare(QStringLiteral("signed_out"), Qt::CaseInsensitive) == 0) {
        return AuthState::SignedOut;
    }
    if (str.compare(QStringLiteral("revoked"), Qt::CaseInsensitive) == 0) {
        return AuthState::Revoked;
    }
    if (str.compare(QStringLiteral("error"), Qt::CaseInsensitive) == 0) {
        return AuthState::Error;
    }
    return AuthState::Unknown;
}

int cwRemoteAccountModel::findAccountById(const QString& accountId) const
{
    for (int i = 0; i < m_entries.count(); ++i) {
        if (m_entries.at(i).accountId == accountId) {
            return i;
        }
    }
    return -1;
}

int cwRemoteAccountModel::findAccount(Provider provider, const QString& username) const
{
    const QString normalizedUsername = username.trimmed();
    for (int i = 0; i < m_entries.count(); ++i) {
        const Entry& entry = m_entries.at(i);
        if (entry.provider == provider && entry.username.compare(normalizedUsername, Qt::CaseInsensitive) == 0) {
            return i;
        }
    }
    return -1;
}

void cwRemoteAccountModel::setAccountActiveByIndex(int targetIndex)
{
    bool changed = false;
    Provider provider = Provider::Unknown;
    if (targetIndex >= 0 && targetIndex < m_entries.size()) {
        provider = m_entries[targetIndex].provider;
    }

    QList<int> changedRows;
    for (int i = 0; i < m_entries.size(); ++i) {
        Entry& entry = m_entries[i];
        if (provider != Provider::Unknown && entry.provider != provider) {
            continue;
        }
        const bool shouldBeActive = (i == targetIndex);
        if (entry.active != shouldBeActive) {
            entry.active = shouldBeActive;
            changed = true;
            changedRows.append(i);
        }
    }

    if (!changed) {
        return;
    }

    for (int rowIndex : changedRows) {
        const QModelIndex row = index(rowIndex, 0);
        emit dataChanged(row, row, { ActiveRole });
    }

    saveToSettings();
    emit accountsChanged();
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
        const QString username = settings.value("username").toString().trimmed();
        if (username.isEmpty()) {
            continue;
        }

        Entry entry;
        entry.accountId = settings.value("accountId").toString().trimmed();
        if (entry.accountId.isEmpty()) {
            entry.accountId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        entry.provider = provider;
        entry.username = username;
        entry.authState = authStateFromString(settings.value("authState").toString());
        entry.active = settings.value("active", false).toBool();
        entry.lastUsedAt = settings.value("lastUsedAt").toDateTime();
        entry.displayName = settings.value("displayName").toString();
        m_entries.append(entry);
    }
    settings.endArray();
    settings.endGroup();

    // Ensure at most one active account per provider.
    QSet<int> seenActiveProviders;
    for (Entry& entry : m_entries) {
        if (!entry.active) {
            continue;
        }
        const int provider = static_cast<int>(entry.provider);
        if (seenActiveProviders.contains(provider)) {
            entry.active = false;
        } else {
            seenActiveProviders.insert(provider);
        }
    }
}

void cwRemoteAccountModel::saveToSettings() const
{
    QSettings settings;
    settings.beginGroup(AccountsGroup);
    settings.beginWriteArray("accounts");
    for (int i = 0; i < m_entries.size(); ++i) {
        settings.setArrayIndex(i);
        const Entry& entry = m_entries.at(i);
        settings.setValue("accountId", entry.accountId);
        settings.setValue("provider", providerToString(entry.provider));
        settings.setValue("username", entry.username);
        settings.setValue("authState", authStateToString(entry.authState));
        settings.setValue("active", entry.active);
        settings.setValue("lastUsedAt", entry.lastUsedAt);
        settings.setValue("displayName", entry.displayName);
    }
    settings.endArray();
    settings.endGroup();
}
