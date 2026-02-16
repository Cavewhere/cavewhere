#include "cwRemoteBindingStore.h"

#include <QSettings>
#include <QUrl>

namespace {
inline constexpr QAnyStringView BindingsGroup = u"RemoteAccountBindings";
inline constexpr QAnyStringView BindingsArray = u"bindings";

QString normalizeRemoteUrlForParsing(const QString& remoteUrl)
{
    QString trimmed = remoteUrl.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    if (!trimmed.contains(QStringLiteral("://"))) {
        const int atIndex = trimmed.indexOf('@');
        const int colonIndex = trimmed.lastIndexOf(':');
        if (atIndex >= 0 && colonIndex > atIndex) {
            return QStringLiteral("ssh://") + trimmed.left(colonIndex) + '/' + trimmed.mid(colonIndex + 1);
        }
        return QStringLiteral("https://") + trimmed;
    }

    return trimmed;
}
}

cwRemoteBindingStore::cwRemoteBindingStore(QObject* parent) :
    QObject(parent)
{
    loadFromSettings();
}

void cwRemoteBindingStore::bindRemoteToAccount(const QString& remoteUrl, const QString& accountId)
{
    const QString remoteKey = canonicalRemoteKey(remoteUrl);
    bindRemoteKeyToAccount(remoteKey, accountId);
}

QString cwRemoteBindingStore::accountIdForRemote(const QString& remoteUrl) const
{
    return accountIdForRemoteKey(canonicalRemoteKey(remoteUrl));
}

void cwRemoteBindingStore::removeBindingsForAccount(const QString& accountId)
{
    const QString normalizedId = accountId.trimmed();
    if (normalizedId.isEmpty()) {
        return;
    }

    bool changed = false;
    for (auto it = m_remoteKeyToAccountId.begin(); it != m_remoteKeyToAccountId.end();) {
        if (it.value() == normalizedId) {
            it = m_remoteKeyToAccountId.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }

    if (changed) {
        saveToSettings();
    }
}

void cwRemoteBindingStore::clear()
{
    if (m_remoteKeyToAccountId.isEmpty()) {
        return;
    }

    m_remoteKeyToAccountId.clear();
    saveToSettings();
}

QString cwRemoteBindingStore::canonicalRemoteKey(const QString& remoteUrl)
{
    const QString normalizedUrl = normalizeRemoteUrlForParsing(remoteUrl);
    if (normalizedUrl.isEmpty()) {
        return {};
    }

    const QUrl parsed = QUrl::fromUserInput(normalizedUrl);
    if (!parsed.isValid()) {
        return {};
    }

    QString host = parsed.host().trimmed().toLower();
    if (host.isEmpty()) {
        return {};
    }

    QString path = parsed.path();
    while (path.startsWith('/')) {
        path.remove(0, 1);
    }
    while (path.endsWith('/')) {
        path.chop(1);
    }
    if (path.isEmpty()) {
        return {};
    }

    const QStringList segments = path.split('/', Qt::SkipEmptyParts);
    if (segments.size() < 2) {
        return {};
    }

    QString owner = segments.at(0).trimmed().toLower();
    QString repo = segments.at(1).trimmed().toLower();
    if (repo.endsWith(QStringLiteral(".git"))) {
        repo.chop(4);
    }

    if (owner.isEmpty() || repo.isEmpty()) {
        return {};
    }

    return host + QStringLiteral("/") + owner + QStringLiteral("/") + repo;
}

void cwRemoteBindingStore::bindRemoteKeyToAccount(const QString& remoteKey, const QString& accountId)
{
    const QString normalizedKey = remoteKey.trimmed().toLower();
    const QString normalizedId = accountId.trimmed();
    if (normalizedKey.isEmpty() || normalizedId.isEmpty()) {
        return;
    }

    if (m_remoteKeyToAccountId.value(normalizedKey) == normalizedId) {
        return;
    }

    m_remoteKeyToAccountId.insert(normalizedKey, normalizedId);
    saveToSettings();
}

QString cwRemoteBindingStore::accountIdForRemoteKey(const QString& remoteKey) const
{
    const QString normalizedKey = remoteKey.trimmed().toLower();
    if (normalizedKey.isEmpty()) {
        return {};
    }
    return m_remoteKeyToAccountId.value(normalizedKey);
}

void cwRemoteBindingStore::loadFromSettings()
{
    m_remoteKeyToAccountId.clear();

    QSettings settings;
    settings.beginGroup(BindingsGroup);
    const int size = settings.beginReadArray(BindingsArray);
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        const QString remoteKey = settings.value("remoteKey").toString().trimmed().toLower();
        const QString accountId = settings.value("accountId").toString().trimmed();
        if (remoteKey.isEmpty() || accountId.isEmpty()) {
            continue;
        }
        m_remoteKeyToAccountId.insert(remoteKey, accountId);
    }
    settings.endArray();
    settings.endGroup();
}

void cwRemoteBindingStore::saveToSettings() const
{
    QSettings settings;
    settings.beginGroup(BindingsGroup);
    settings.beginWriteArray(BindingsArray);

    int index = 0;
    for (auto it = m_remoteKeyToAccountId.cbegin(); it != m_remoteKeyToAccountId.cend(); ++it, ++index) {
        settings.setArrayIndex(index);
        settings.setValue("remoteKey", it.key());
        settings.setValue("accountId", it.value());
    }

    settings.endArray();
    settings.endGroup();
}
