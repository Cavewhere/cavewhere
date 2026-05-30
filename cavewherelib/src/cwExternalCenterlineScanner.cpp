/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwExternalCenterlineScanner.h"

//Walls
#include "wallsprojectparser.h"

//Qt includes
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QSharedPointer>
#include <QStringConverter>
#include <QStringDecoder>

namespace {

constexpr const char* kSurvexCommentChar = ";";
constexpr const char* kSurvexExtension = ".svx";
constexpr const char* kCompassDatExtension = ".dat";
constexpr const char* kCompassMakExtension = ".mak";
constexpr const char* kWallsWpjExtension = ".wpj";
constexpr const char* kWallsSrvExtension = ".srv";

// UTF-8 byte-order mark (0xEF 0xBB 0xBF).
const QByteArray kUtf8Bom = QByteArray::fromHex("EFBBBF");

bool hasExtension(const QString& path, const char* extension)
{
    return path.endsWith(QLatin1String(extension), Qt::CaseInsensitive);
}

QString canonicalize(const QString& path)
{
    const QString canonical = QFileInfo(path).canonicalFilePath();
    return canonical.isEmpty() ? QString() : canonical;
}

/**
 * Resolves a relative or absolute include target against the
 * directory holding the file that wrote the *include directive.
 * On case-sensitive filesystems (Linux), if the target doesn't
 * match as-written, retries with a case-insensitive directory
 * scan. Returns:
 *   - resolved canonical path (non-empty) and matchedAs unchanged
 *     when the literal path was found
 *   - resolved canonical path (non-empty) and matchedAs set to
 *     the actual on-disk filename when the case-fallback hit
 *   - empty resolved path when nothing matched
 */
struct IncludeResolveResult {
    QString resolved;
    QString matchedAs;
};

IncludeResolveResult resolveIncludeTarget(const QString& target, const QDir& baseDir)
{
    const QString absoluteAsWritten = QFileInfo(target).isAbsolute()
        ? target
        : baseDir.absoluteFilePath(target);

    if (QFileInfo::exists(absoluteAsWritten)) {
        return {canonicalize(absoluteAsWritten), QString()};
    }

    // Case-fallback: list the parent directory and look for a
    // lowercase-equal filename. On case-insensitive filesystems
    // (macOS APFS default, NTFS, FAT) the literal lookup above
    // already succeeded, so this branch is Linux-only.
    const QFileInfo info(absoluteAsWritten);
    const QDir parent = info.absoluteDir();
    if (!parent.exists()) {
        return {QString(), QString()};
    }

    const QString needle = info.fileName().toLower();
    // Sort by name so that, when two files differ only in case
    // ("Foo.svx" vs "FOO.svx" on a Linux filesystem), the first
    // match is deterministic across runs and platforms rather than
    // depending on filesystem inode order.
    const QStringList entries =
        parent.entryList(QDir::Files, QDir::Name | QDir::IgnoreCase);
    for (const QString& entry : entries) {
        if (entry.toLower() == needle) {
            const QString candidate = parent.absoluteFilePath(entry);
            return {canonicalize(candidate), entry};
        }
    }

    return {QString(), QString()};
}

struct DecodedFile {
    QString text;
    bool usedLatin1Fallback = false;
    bool decoded = true;
};

DecodedFile decodeBytes(QByteArray bytes)
{
    if (bytes.startsWith(kUtf8Bom)) {
        bytes.remove(0, kUtf8Bom.size());
    }

    // Stateless only - we drive a single call per file and rely on
    // hasError() to flip when the byte stream isn't valid UTF-8.
    // Don't set ConvertInvalidToNull: it would silently rewrite
    // invalid sequences to U+FFFD and could mask the fallback trigger.
    QStringDecoder decoder(QStringConverter::Utf8,
                           QStringDecoder::Flag::Stateless);
    QString text = decoder.decode(bytes);
    if (!decoder.hasError()) {
        return {std::move(text), false, true};
    }

    // Survex historically tolerated Latin-1 input on systems
    // without UTF-8 locale support; mirror that here so projects
    // authored before UTF-8 was the default still scan.
    return {QString::fromLatin1(bytes), true, true};
}

// Match: *include "path with spaces.svx"  OR  *include path/no-spaces.svx
// followed by optional trailing whitespace + optional ';' comment.
// Anchored to start-of-line; intentionally NOT case-sensitive
// (Survex commands are case-insensitive).
const QRegularExpression& survexIncludeRegex()
{
    static const QRegularExpression regex(
        QString::fromLatin1(R"RX(^\s*\*include\s+(?:"([^"]+)"|(\S+))\s*(?:;.*)?$)RX"),
        QRegularExpression::CaseInsensitiveOption);
    return regex;
}

/**
 * Strips a trailing Survex comment ("; ...") and any leading /
 * trailing whitespace. Used as a pre-step before regex matching so
 * we never confuse a commented-out include with a real one.
 */
QString stripCommentAndWhitespace(const QString& line)
{
    const int commentStart = line.indexOf(QLatin1Char(';'));
    QString trimmed = commentStart >= 0 ? line.left(commentStart) : line;
    return trimmed.trimmed();
}

QString includeTargetWithExtension(const QString& target)
{
    if (QFileInfo(target).suffix().isEmpty()) {
        return target + QLatin1String(kSurvexExtension);
    }
    return target;
}

struct ScanState {
    // canonical paths fully processed; re-includes are silently
    // deduplicated when the file is here but NOT in inProgress
    QSet<QString> visited;
    // canonical paths currently on the recursion stack; a re-entry
    // into one of these is a real cycle and must warn
    QSet<QString> inProgress;
    QStringList dependencies;  // in walk order, deduplicated
    QStringList warnings;
};

void scanSurvexFile(const QString& filePath, ScanState& state);
void scanCompassFile(const QString& filePath, ScanState& state);
void scanWallsFile(const QString& filePath, ScanState& state);
void scanByFormat(const QString& filePath, ScanState& state);

void recordWarning(ScanState& state, const QString& message)
{
    state.warnings.append(message);
}

void scanSurvexFile(const QString& filePath, ScanState& state)
{
    const QString canonical = canonicalize(filePath);
    if (canonical.isEmpty()) {
        recordWarning(state,
                      QStringLiteral("include target not found: %1").arg(filePath));
        return;
    }

    if (state.inProgress.contains(canonical)) {
        // A re-entry into a file already on the recursion stack is a
        // true cycle - warn and stop expanding this branch.
        recordWarning(state,
                      QStringLiteral("circular include skipped: %1").arg(filePath));
        return;
    }
    if (state.visited.contains(canonical)) {
        // Already fully processed via another include path. Silent
        // dedup - dependencies already lists it.
        return;
    }

    QFile file(canonical);
    if (!file.open(QFile::ReadOnly)) {
        recordWarning(state,
                      QStringLiteral("cannot read %1: %2")
                          .arg(canonical, file.errorString()));
        return;
    }
    const QByteArray bytes = file.readAll();
    file.close();

    state.inProgress.insert(canonical);
    state.dependencies.append(canonical);

    const DecodedFile decoded = decodeBytes(bytes);
    if (decoded.usedLatin1Fallback) {
        recordWarning(state,
                      QStringLiteral("encoding fallback (UTF-8 invalid, decoded as Latin-1): %1")
                          .arg(canonical));
    }

    const QDir baseDir = QFileInfo(canonical).absoluteDir();
    const QRegularExpression& regex = survexIncludeRegex();
    const QStringList lines = decoded.text.split(QLatin1Char('\n'));
    for (const QString& rawLine : lines) {
        const QString line = stripCommentAndWhitespace(rawLine);
        if (line.isEmpty()) {
            continue;
        }
        // Quick rejection: commands must start with '*'. Cuts the
        // regex out of the hot path for the common no-directive
        // line.
        if (!line.startsWith(QLatin1Char('*'))) {
            continue;
        }

        const auto match = regex.match(line);
        if (!match.hasMatch()) {
            continue;
        }

        QString target = match.captured(1);  // double-quoted form
        if (target.isEmpty()) {
            target = match.captured(2);      // bareword form
        }
        if (target.isEmpty()) {
            continue;
        }

        target = includeTargetWithExtension(target);

        const IncludeResolveResult resolution = resolveIncludeTarget(target, baseDir);
        if (resolution.resolved.isEmpty()) {
            recordWarning(state,
                          QStringLiteral("missing *include target: %1").arg(target));
            continue;
        }
        if (!resolution.matchedAs.isEmpty()
            && resolution.matchedAs != QFileInfo(target).fileName()) {
            recordWarning(state,
                          QStringLiteral("case-fallback match: include '%1' resolved to '%2'")
                              .arg(QFileInfo(target).fileName(), resolution.matchedAs));
        }

        // Cross-format dispatch: Survex's *include can pull in .dat /
        // .mak / .wpj / .srv files. Per the plan, cavern auto-detects
        // format at solve time, so the scanner mirrors that.
        scanByFormat(resolution.resolved, state);
    }

    state.inProgress.remove(canonical);
    state.visited.insert(canonical);
}

// Compass .mak references look like:
//   #filename.dat,A,B;          // bareword form
//   #"name with spaces.dat",A;  // quoted form (Windows projects use this)
//   /comment                    // single-line comment
//   "Cave Name";                // misc directive
// We only care about '#'-prefixed lines. Group 1 captures the
// double-quoted filename, group 2 the bareword.
const QRegularExpression& compassMakReferenceRegex()
{
    static const QRegularExpression regex(
        QString::fromLatin1(R"RX(^\s*#\s*(?:"([^"]+)"|([^,;\s]+)))RX"),
        QRegularExpression::CaseInsensitiveOption);
    return regex;
}

QString stripCompassComment(const QString& line)
{
    const int slashStart = line.indexOf(QLatin1Char('/'));
    if (slashStart < 0) {
        return line;
    }
    return line.left(slashStart);
}

void scanCompassFile(const QString& filePath, ScanState& state)
{
    const QString canonical = canonicalize(filePath);
    if (canonical.isEmpty()) {
        recordWarning(state,
                      QStringLiteral("Compass entry not found: %1").arg(filePath));
        return;
    }

    if (state.inProgress.contains(canonical)) {
        recordWarning(state,
                      QStringLiteral("circular include skipped: %1").arg(filePath));
        return;
    }
    if (state.visited.contains(canonical)) {
        return;
    }

    const bool isMak = hasExtension(canonical, kCompassMakExtension);

    state.inProgress.insert(canonical);
    state.dependencies.append(canonical);

    if (isMak) {
        QFile file(canonical);
        if (!file.open(QFile::ReadOnly)) {
            recordWarning(state,
                          QStringLiteral("cannot read %1: %2")
                              .arg(canonical, file.errorString()));
        } else {
            const QByteArray bytes = file.readAll();
            file.close();

            const DecodedFile decoded = decodeBytes(bytes);
            if (decoded.usedLatin1Fallback) {
                recordWarning(state,
                              QStringLiteral("encoding fallback (UTF-8 invalid, decoded as Latin-1): %1")
                                  .arg(canonical));
            }

            const QDir baseDir = QFileInfo(canonical).absoluteDir();
            const QRegularExpression& regex = compassMakReferenceRegex();
            const QStringList lines = decoded.text.split(QLatin1Char('\n'));
            for (const QString& rawLine : lines) {
                const QString line = stripCompassComment(rawLine).trimmed();
                if (line.isEmpty() || !line.startsWith(QLatin1Char('#'))) {
                    continue;
                }
                const auto match = regex.match(line);
                if (!match.hasMatch()) {
                    continue;
                }
                QString target = match.captured(1);  // quoted form
                if (target.isEmpty()) {
                    target = match.captured(2);      // bareword form
                }
                if (target.isEmpty()) {
                    continue;
                }
                const IncludeResolveResult resolution =
                    resolveIncludeTarget(target, baseDir);
                if (resolution.resolved.isEmpty()) {
                    recordWarning(state,
                                  QStringLiteral("missing Compass reference: %1").arg(target));
                    continue;
                }
                if (!resolution.matchedAs.isEmpty()
                    && resolution.matchedAs != QFileInfo(target).fileName()) {
                    recordWarning(state,
                                  QStringLiteral("case-fallback match: include '%1' resolved to '%2'")
                                      .arg(QFileInfo(target).fileName(), resolution.matchedAs));
                }
                // .mak entries point at .dat files, which never include
                // anything else - but go through scanByFormat anyway so
                // a .mak that nests another .mak still walks correctly.
                scanByFormat(resolution.resolved, state);
            }
        }
    }

    state.inProgress.remove(canonical);
    state.visited.insert(canonical);
}

void collectWallsSurveys(const dewalls::WpjBookPtr& book,
                        const QDir& baseDir,
                        ScanState& state)
{
    if (book.isNull()) {
        return;
    }
    for (const auto& child : book->Children) {
        if (child.isNull()) {
            continue;
        }
        if (child->isBook()) {
            collectWallsSurveys(child.dynamicCast<dewalls::WpjBook>(), baseDir, state);
            continue;
        }
        // .SURVEY / .OTHER both reference an external file by Name.
        if (child->Name.isEmpty()) {
            continue;
        }
        QString absolutePath = child->absolutePath();
        if (absolutePath.isEmpty()) {
            absolutePath = baseDir.absoluteFilePath(child->Name.value());
        }
        // dewalls' absolutePath() returns just the file stem if the
        // entry doesn't carry an extension; .SURVEY entries default
        // to .srv on disk per Walls semantics.
        if (QFileInfo(absolutePath).suffix().isEmpty()) {
            absolutePath += QLatin1String(kWallsSrvExtension);
        }
        if (!QFileInfo::exists(absolutePath)) {
            recordWarning(state,
                          QStringLiteral("missing Walls reference: %1").arg(absolutePath));
            continue;
        }
        scanByFormat(absolutePath, state);
    }
}

void scanWallsFile(const QString& filePath, ScanState& state)
{
    const QString canonical = canonicalize(filePath);
    if (canonical.isEmpty()) {
        recordWarning(state,
                      QStringLiteral("Walls entry not found: %1").arg(filePath));
        return;
    }

    if (state.inProgress.contains(canonical)) {
        recordWarning(state,
                      QStringLiteral("circular include skipped: %1").arg(filePath));
        return;
    }
    if (state.visited.contains(canonical)) {
        return;
    }

    const bool isWpj = hasExtension(canonical, kWallsWpjExtension);

    if (isWpj) {
        // .wpj: parse first; if dewalls returns a null root the file
        // exists on disk but isn't usable to cavern, so drop it from
        // the dependency list with a warning rather than carrying a
        // broken project file into reconcile.
        dewalls::WallsProjectParser parser;
        dewalls::WpjBookPtr root = parser.parseFile(canonical);
        if (root.isNull()) {
            recordWarning(state,
                          QStringLiteral("could not parse Walls project: %1").arg(canonical));
            return;
        }

        state.inProgress.insert(canonical);
        state.dependencies.append(canonical);

        const QDir baseDir = QFileInfo(canonical).absoluteDir();
        collectWallsSurveys(root, baseDir, state);
    } else {
        // Bare .srv: trust the file as-is, no parsing needed.
        state.inProgress.insert(canonical);
        state.dependencies.append(canonical);
    }

    state.inProgress.remove(canonical);
    state.visited.insert(canonical);
}

void scanByFormat(const QString& filePath, ScanState& state)
{
    using namespace cwExternalCenterlineScanner;
    const Format fmt = formatFor(filePath);
    switch (fmt) {
    case Format::Survex:
        scanSurvexFile(filePath, state);
        return;
    case Format::Compass:
        scanCompassFile(filePath, state);
        return;
    case Format::Walls:
        scanWallsFile(filePath, state);
        return;
    case Format::Unknown:
        recordWarning(state,
                      QStringLiteral("include target has unknown format: %1").arg(filePath));
        return;
    }
}

} // namespace

namespace cwExternalCenterlineScanner {

Format formatFor(const QString& entryFile)
{
    if (hasExtension(entryFile, kSurvexExtension)) {
        return Format::Survex;
    }
    if (hasExtension(entryFile, kCompassDatExtension)
        || hasExtension(entryFile, kCompassMakExtension)) {
        return Format::Compass;
    }
    if (hasExtension(entryFile, kWallsWpjExtension)
        || hasExtension(entryFile, kWallsSrvExtension)) {
        return Format::Walls;
    }
    return Format::Unknown;
}

Monad::Result<ScanResult> scan(const QString& entryFile)
{
    const Format format = formatFor(entryFile);
    switch (format) {
    case Format::Survex:
        return scanSurvex(entryFile);
    case Format::Compass:
        return scanCompass(entryFile);
    case Format::Walls:
        return scanWalls(entryFile);
    case Format::Unknown:
        return Monad::Result<ScanResult>(
            QStringLiteral("scanner: cannot determine format from extension: %1")
                .arg(entryFile));
    }
    return Monad::Result<ScanResult>(
        QStringLiteral("scanner: unhandled format for %1").arg(entryFile));
}

namespace {

Monad::Result<ScanResult> scanWithEntry(const QString& entryFile,
                                        void (*scanFn)(const QString&, ScanState&),
                                        const char* contextName)
{
    if (entryFile.isEmpty()) {
        return Monad::Result<ScanResult>(
            QStringLiteral("%1: empty entry file").arg(QLatin1String(contextName)));
    }
    if (!QFileInfo::exists(entryFile)) {
        return Monad::Result<ScanResult>(
            QStringLiteral("%1: entry file does not exist: %2")
                .arg(QLatin1String(contextName), entryFile));
    }

    ScanState state;
    scanFn(entryFile, state);

    if (state.dependencies.isEmpty()) {
        return Monad::Result<ScanResult>(
            QStringLiteral("%1: entry file could not be read: %2")
                .arg(QLatin1String(contextName), entryFile));
    }

    ScanResult result;
    result.dependencies = state.dependencies;
    result.warnings = state.warnings;
    return Monad::Result<ScanResult>(result);
}

} // namespace

Monad::Result<ScanResult> scanSurvex(const QString& entryFile)
{
    return scanWithEntry(entryFile, &scanSurvexFile, "scanSurvex");
}

Monad::Result<ScanResult> scanCompass(const QString& entryFile)
{
    return scanWithEntry(entryFile, &scanCompassFile, "scanCompass");
}

Monad::Result<ScanResult> scanWalls(const QString& entryFile)
{
    return scanWithEntry(entryFile, &scanWallsFile, "scanWalls");
}

} // namespace cwExternalCenterlineScanner
