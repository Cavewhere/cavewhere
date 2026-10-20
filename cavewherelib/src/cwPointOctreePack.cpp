// cwPointOctreePack.cpp
#include "cwPointOctreePack.h"

//Qt includes
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include <QSet>
#include <QtEndian>

//Std includes
#include <algorithm>
#include <optional>

namespace {
    constexpr char kMagic[] = {'C', 'W', 'P', 'K'};
    constexpr qint64 kMagicBytes = sizeof(kMagic);
    constexpr quint32 kVersion = 1;
    constexpr qint64 kU64Bytes = sizeof(quint64);
    constexpr qint64 kU32Bytes = sizeof(quint32);

    constexpr qint64 kHeaderBytes = kMagicBytes + kU32Bytes;
    constexpr qint64 kIndexEntryBytes = 2 * kU64Bytes;
    constexpr qint64 kChecksumBytes = kU64Bytes;

    //manifestOffset, manifestSize, indexOffset, nodeCount, fileSize, checksum,
    //then the trailing magic and version
    constexpr qint64 kFooterManifestOffset = 0;
    constexpr qint64 kFooterManifestSize = kFooterManifestOffset + kU64Bytes;
    constexpr qint64 kFooterIndexOffset = kFooterManifestSize + kU64Bytes;
    constexpr qint64 kFooterNodeCount = kFooterIndexOffset + kU64Bytes;
    constexpr qint64 kFooterFileSize = kFooterNodeCount + kU64Bytes;
    constexpr qint64 kFooterChecksum = kFooterFileSize + kU64Bytes;
    constexpr qint64 kFooterMagic = kFooterChecksum + kChecksumBytes;
    constexpr qint64 kFooterBytes = kFooterMagic + kHeaderBytes;
    static_assert(kFooterBytes == 56, "The plan fixes the footer at 56 bytes");

    constexpr QLatin1StringView kTempSuffix(".tmp");

    void appendU64(QByteArray& bytes, quint64 value)
    {
        char buffer[kU64Bytes];
        qToLittleEndian(value, buffer);
        bytes.append(buffer, kU64Bytes);
    }

    QByteArray magicAndVersion()
    {
        QByteArray bytes(kMagic, kMagicBytes);
        char buffer[kU32Bytes];
        qToLittleEndian(kVersion, buffer);
        bytes.append(buffer, kU32Bytes);
        return bytes;
    }

    bool hasMagicAndVersion(const uchar* at)
    {
        return std::equal(kMagic, kMagic + kMagicBytes, reinterpret_cast<const char*>(at))
               && qFromLittleEndian<quint32>(at + kMagicBytes) == kVersion;
    }

    QByteArray checksum(QByteArrayView manifest, QByteArrayView index)
    {
        QCryptographicHash hash(QCryptographicHash::Sha1);
        hash.addData(manifest);
        hash.addData(index);
        return hash.result().left(kChecksumBytes);
    }
}

cwPointOctreePack::cwPointOctreePack(const QString& packPath) :
    m_file(packPath)
{
}

std::shared_ptr<const cwPointOctreePack> cwPointOctreePack::open(const QString& packPath)
{
    std::shared_ptr<cwPointOctreePack> pack(new cwPointOctreePack(packPath));
    if(pack->load()) {
        return pack;
    }
    return nullptr;
}

bool cwPointOctreePack::load()
{
    if(!m_file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const qint64 size = m_file.size();
    if(size < kHeaderBytes + kFooterBytes) {
        return false;
    }

    m_mapped = m_file.map(0, size);
    //The mapping outlives the descriptor (QFileDevice::map)
    m_file.close();
    if(m_mapped == nullptr) {
        return false;
    }

    const uchar* footer = m_mapped + size - kFooterBytes;
    if(!hasMagicAndVersion(m_mapped) || !hasMagicAndVersion(footer + kFooterMagic)) {
        return false;
    }

    const quint64 fileSize = qFromLittleEndian<quint64>(footer + kFooterFileSize);
    if(fileSize != quint64(size)) {
        return false;
    }

    const quint64 footerOffset = quint64(size - kFooterBytes);
    const quint64 manifestOffset = qFromLittleEndian<quint64>(footer + kFooterManifestOffset);
    const quint64 manifestSize = qFromLittleEndian<quint64>(footer + kFooterManifestSize);
    const quint64 indexOffset = qFromLittleEndian<quint64>(footer + kFooterIndexOffset);
    const quint64 nodeCount = qFromLittleEndian<quint64>(footer + kFooterNodeCount);

    //Every region lies inside the file, in order, with no gap before the footer
    if(manifestOffset < quint64(kHeaderBytes)
       || manifestOffset > indexOffset
       || manifestSize != indexOffset - manifestOffset
       || indexOffset > footerOffset
       || nodeCount != (footerOffset - indexOffset) / quint64(kIndexEntryBytes)
       || (footerOffset - indexOffset) % quint64(kIndexEntryBytes) != 0) {
        return false;
    }

    const QByteArrayView manifestBytes(m_mapped + manifestOffset, qsizetype(manifestSize));
    const QByteArrayView indexBytes(m_mapped + indexOffset, qsizetype(footerOffset - indexOffset));
    const QByteArrayView storedChecksum(footer + kFooterChecksum, kChecksumBytes);
    if(checksum(manifestBytes, indexBytes) != storedChecksum) {
        return false;
    }

    m_index.reserve(qsizetype(nodeCount));
    for(quint64 i = 0; i < nodeCount; i++) {
        const uchar* entry = m_mapped + indexOffset + i * quint64(kIndexEntryBytes);
        const IndexEntry indexEntry {
            qFromLittleEndian<quint64>(entry),
            qFromLittleEndian<quint64>(entry + kU64Bytes)
        };
        if(indexEntry.offset < quint64(kHeaderBytes)
           || indexEntry.size > manifestOffset
           || indexEntry.offset > manifestOffset - indexEntry.size) {
            return false;
        }
        m_index.append(indexEntry);
    }

    std::optional<cwPointOctreeManifest> manifest =
        cwPointOctreeManifest::deserialize(manifestBytes.toByteArray());
    if(!manifest.has_value() || quint64(manifest->nodes.size()) != nodeCount) {
        return false;
    }
    m_manifest = std::move(*manifest);
    return true;
}

const cwPointOctreeManifest& cwPointOctreePack::manifest() const
{
    return m_manifest;
}

QByteArray cwPointOctreePack::node(int index) const
{
    if(index < 0 || index >= m_index.size()) {
        return QByteArray();
    }
    const IndexEntry& entry = m_index.at(index);
    return QByteArray(reinterpret_cast<const char*>(m_mapped + entry.offset), qsizetype(entry.size));
}

int cwPointOctreePack::nodeCount() const
{
    return int(m_index.size());
}

QString cwPointOctreePack::path() const
{
    return m_file.fileName();
}

cwPointOctreePack::Writer::Writer(const QString& packPath) :
    m_packPath(packPath),
    m_file(packPath + kTempSuffix)
{
    if(!QDir().mkpath(QFileInfo(packPath).path())) {
        m_errorMessage = QStringLiteral("Couldn't create the directory of %1").arg(packPath);
        return;
    }

    if(!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_errorMessage = QStringLiteral("Couldn't open %1: %2").arg(m_file.fileName(), m_file.errorString());
        return;
    }

    if(!writeBytes(magicAndVersion())) {
        m_file.close();
    }
}

cwPointOctreePack::Writer::~Writer()
{
    if(!m_finished) {
        m_file.close();
        m_file.remove();
    }
}

bool cwPointOctreePack::Writer::isOpen() const
{
    QMutexLocker locker(&m_mutex);
    return m_file.isOpen();
}

bool cwPointOctreePack::Writer::writeBytes(const QByteArray& bytes)
{
    if(m_file.write(bytes) != bytes.size()) {
        return fail(QStringLiteral("Couldn't write %1: %2").arg(m_file.fileName(), m_file.errorString()));
    }
    m_end += bytes.size();
    return true;
}

bool cwPointOctreePack::Writer::fail(const QString& message)
{
    if(m_errorMessage.isEmpty()) {
        m_errorMessage = message;
    }
    return false;
}

bool cwPointOctreePack::Writer::append(quint64 cellKey, const QByteArray& payload)
{
    QMutexLocker locker(&m_mutex);
    if(!m_file.isOpen()) {
        return fail(QStringLiteral("%1 isn't open").arg(m_file.fileName()));
    }

    if(m_entries.contains(cellKey)) {
        return fail(QStringLiteral("Cell %1 was appended twice to %2").arg(cellKey).arg(m_packPath));
    }

    const qint64 offset = m_end;
    if(!writeBytes(payload)) {
        return false;
    }
    m_entries.insert(cellKey, IndexEntry {quint64(offset), quint64(payload.size())});
    return true;
}

bool cwPointOctreePack::Writer::finish(const cwPointOctreeManifest& manifest)
{
    QMutexLocker locker(&m_mutex);

    const auto writePack = [&]() {
        if(!m_errorMessage.isEmpty()) {
            return false;
        }
        if(!m_file.isOpen()) {
            return fail(QStringLiteral("%1 isn't open").arg(m_file.fileName()));
        }

        QByteArray index;
        index.reserve(manifest.nodes.size() * kIndexEntryBytes);
        QSet<quint64> listed;
        listed.reserve(manifest.nodes.size());
        for(int i = 0; i < manifest.nodes.size(); i++) {
            const quint64 key = cw::octree::cellKey(manifest.nodes.at(i));
            const auto entry = m_entries.constFind(key);
            if(entry == m_entries.cend()) {
                return fail(QStringLiteral("Node %1 has no payload in %2").arg(manifest.nodeName(i), m_packPath));
            }
            if(listed.contains(key)) {
                return fail(QStringLiteral("The manifest lists cell %1 twice").arg(key));
            }
            listed.insert(key);
            appendU64(index, entry->offset);
            appendU64(index, entry->size);
        }

        if(listed.size() != m_entries.size()) {
            return fail(QStringLiteral("%1 payloads in %2 have no manifest node")
                            .arg(m_entries.size() - listed.size())
                            .arg(m_packPath));
        }

        const QByteArray manifestBytes = manifest.serialize();
        const qint64 manifestOffset = m_end;
        const qint64 indexOffset = manifestOffset + manifestBytes.size();
        const qint64 fileSize = indexOffset + index.size() + kFooterBytes;

        QByteArray footer;
        footer.reserve(kFooterBytes);
        appendU64(footer, quint64(manifestOffset));
        appendU64(footer, quint64(manifestBytes.size()));
        appendU64(footer, quint64(indexOffset));
        appendU64(footer, quint64(manifest.nodes.size()));
        appendU64(footer, quint64(fileSize));
        footer.append(checksum(manifestBytes, index));
        footer.append(magicAndVersion());

        if(!writeBytes(manifestBytes) || !writeBytes(index) || !writeBytes(footer)) {
            return false;
        }

        if(!m_file.flush()) {
            return fail(QStringLiteral("Couldn't flush %1: %2").arg(m_file.fileName(), m_file.errorString()));
        }
        m_file.close();
        if(m_file.error() != QFileDevice::NoError) {
            return fail(QStringLiteral("Couldn't close %1: %2").arg(m_file.fileName(), m_file.errorString()));
        }

        //Through QFile, not std::filesystem, so iOS's security-scoped file
        //engine grants access to a project picked from Files; m_file stays
        //alive past the rename because the engine releases its scope in its
        //destructor
        QFile::remove(m_packPath);
        if(!m_file.rename(m_packPath)) {
            return fail(QStringLiteral("Couldn't rename %1 to %2: %3")
                            .arg(m_file.fileName(), m_packPath, m_file.errorString()));
        }
        return true;
    };

    m_finished = writePack();
    if(!m_finished) {
        m_file.close();
        m_file.remove();
    }
    return m_finished;
}

QString cwPointOctreePack::Writer::errorMessage() const
{
    QMutexLocker locker(&m_mutex);
    return m_errorMessage;
}
