#include "cwZip.h"

// Qt
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QByteArray>
#include <QDebug>
#include <functional>

// minizip-ng (local to implementation)
#include <mz.h>
#include <minizip-ng/mz_strm.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>

using namespace Monad;

static QString toErrorText(int32_t code) {
    switch (code) {
    case MZ_OK: return QStringLiteral("OK");
    case MZ_STREAM_ERROR: return QStringLiteral("STREAM_ERROR");
    case MZ_DATA_ERROR: return QStringLiteral("DATA_ERROR");
    case MZ_MEM_ERROR: return QStringLiteral("MEM_ERROR");
    case MZ_BUF_ERROR: return QStringLiteral("BUF_ERROR");
    case MZ_VERSION_ERROR: return QStringLiteral("VERSION_ERROR");
    case MZ_END_OF_LIST: return QStringLiteral("END_OF_LIST");
    case MZ_PARAM_ERROR: return QStringLiteral("PARAM_ERROR");
    case MZ_FORMAT_ERROR: return QStringLiteral("FORMAT_ERROR");
    case MZ_INTERNAL_ERROR: return QStringLiteral("INTERNAL_ERROR");
    case MZ_CRC_ERROR: return QStringLiteral("CRC_ERROR");
    default: return QStringLiteral("ERR(%1)").arg(code);
    }
}

static QString cwzip_normalizeRelPath(const QString& relPath) {
    // ZIP uses forward slashes; ensure no leading "./" and no backslashes.
    QString p = relPath;
    p.replace('\\', '/');
    while (p.startsWith("./")) {
        p.remove(0, 2);
    }
    return p;
}


bool cwZip::ensureParentDirectory(const QString& fullPath)
{
    const QString parent = QFileInfo(fullPath).path();
    return QDir().mkpath(parent);
}


// -----------------------------
// Extraction (Monad::Result)
// -----------------------------
Monad::Result<cwZip::ZipResult> cwZip::extractAll(const QString& zipFilePath,
                                           const QString& outputDirectory)
{

    struct ZipReader {
        void* handle = nullptr;
        bool opened = false;
        ~ZipReader() {
            if (handle != nullptr) {
                if (opened) {
                    mz_zip_reader_close(handle);
                }
                mz_zip_reader_delete(&handle);
            }
        }
    } reader;

    reader.handle = mz_zip_reader_create();
    if (reader.handle == nullptr) {
        return Monad::Result<ZipResult>(QStringLiteral("Failed to create zip reader"),
                                        Monad::ResultBase::Unknown);
    }

    {
        const QByteArray zipPathBytes = zipFilePath.toUtf8();
        const int32_t openStatus = mz_zip_reader_open_file(reader.handle, zipPathBytes.constData());
        if (openStatus != MZ_OK) {
            return Monad::Result<ZipResult>(
                QStringLiteral("Failed to open zip \"%1\": %2")
                    .arg(zipFilePath, toErrorText(openStatus)),
                openStatus
                );
        }
        reader.opened = true;
    }

    ZipResult result; // your struct that tracks filesExtracted, directoriesCreated, hadErrors

    // Position at first entry
    int32_t status = mz_zip_reader_goto_first_entry(reader.handle);
    if (status != MZ_OK && status != MZ_END_OF_LIST) {
        return Monad::Result<ZipResult>(
            QStringLiteral("Failed to read first entry in \"%1\": %2")
                .arg(zipFilePath, toErrorText(status)),
            status
            );
    }

    while (status == MZ_OK) {
        mz_zip_file* fileInfo = nullptr;
        if (mz_zip_reader_entry_get_info(reader.handle, &fileInfo) != MZ_OK || fileInfo == nullptr) {
            result.hadErrors = true;
        } else {
            const QString entryName = QString::fromUtf8(fileInfo->filename ? fileInfo->filename : "");
            const bool isDirectory = entryName.endsWith('/');
            // const bool isDirectory = (fileInfo->flag & MZ_ZIP_FLAG_IS_DIRECTORY) != 0;
            const QString outputPath = QDir(outputDirectory).filePath(entryName);

            if (isDirectory) {
                if (QDir().mkpath(outputPath)) {
                    result.directoriesCreated++;
                } else {
                    result.hadErrors = true;
                }
            } else {
                if (!ensureParentDirectory(outputPath)) {
                    result.hadErrors = true;
                } else {
                    const int32_t openEntryStatus = mz_zip_reader_entry_open(reader.handle);
                    if (openEntryStatus == MZ_OK) {
                        QFile outFile(outputPath);
                        if (outFile.open(QIODevice::WriteOnly)) {
                            constexpr int bufferSize = 128 * 1024;
                            QByteArray buffer(bufferSize, Qt::Uninitialized);

                            int32_t bytesRead = 0;
                            do {
                                bytesRead = mz_zip_reader_entry_read(reader.handle,
                                                                     buffer.data(),
                                                                     buffer.size());
                                if (bytesRead > 0) {
                                    const qint64 written = outFile.write(buffer.constData(), bytesRead);
                                    if (written != bytesRead) {
                                        result.hadErrors = true;
                                        break;
                                    }
                                } else if (bytesRead < 0) {
                                    result.hadErrors = true;
                                    break;
                                }
                            } while (bytesRead > 0);

                            outFile.close();

                            const int32_t closeEntryStatus = mz_zip_reader_entry_close(reader.handle);
                            if (closeEntryStatus == MZ_OK && !result.hadErrors) {
                                result.filesExtracted++;
                            } else {
                                result.hadErrors = true;
                            }
                        } else {
                            result.hadErrors = true;
                            mz_zip_reader_entry_close(reader.handle); // best effort close
                        }
                    } else {
                        result.hadErrors = true;
                    }
                }
            }
        }

        status = mz_zip_reader_goto_next_entry(reader.handle);
        if (status != MZ_OK && status != MZ_END_OF_LIST) {
            // Fatal traversal error: abort
            return Monad::Result<ZipResult>(
                QStringLiteral("Failed while advancing entries in \"%1\": %2")
                    .arg(zipFilePath, toErrorText(status)),
                status
                );
        }
    }

    // Reader closed by ZipReader dtor

    if (result.hadErrors) {
        return Monad::Result<ZipResult>(result,
                                        QStringLiteral("Completed with some errors while extracting \"%1\"").arg(zipFilePath));
        // This uses the Warning pathway in Monad::Result<T>
    } else {
        return Monad::Result<ZipResult>(result);
    }
}


// -----------------------------
// Compression (STREAMING)
// -----------------------------
// Return: NoError on success,
//         Warning if some files failed but the archive is still created,
//         Error (with minizip code when available) on fatal failure.
Monad::ResultBase cwZip::zipDirectory(const QString& sourceDirPath, const QString& zipFilePath)
{

    QDir sourceDir(sourceDirPath);
    if (!sourceDir.exists()) {
        return Monad::ResultBase(
            QStringLiteral("Source directory does not exist: %1").arg(sourceDirPath),
            Monad::ResultBase::CustomError
            );
    }

    // RAII for writer handle
    struct Writer {
        void* handle = nullptr;
        bool opened = false;
        ~Writer() {
            if (handle != nullptr) {
                if (opened) {
                    mz_zip_writer_close(handle);
                }
                mz_zip_writer_delete(&handle);
            }
        }
    } writer;

    writer.handle = mz_zip_writer_create();
    if (writer.handle == nullptr) {
        return Monad::ResultBase(
            QStringLiteral("Failed to create zip writer"),
            Monad::ResultBase::Unknown
            );
    }

    {
        const QByteArray zipUtf8 = zipFilePath.toUtf8();
        const int32_t openStatus = mz_zip_writer_open_file(writer.handle, zipUtf8.constData(), 0, 0);
        if (openStatus != MZ_OK) {
            return Monad::ResultBase(
                QStringLiteral("Could not create zip file \"%1\": %2")
                    .arg(zipFilePath, toErrorText(openStatus)),
                openStatus
                );
        }
        writer.opened = true;
    }

    // Collect non-fatal issues as warnings
    QStringList warnings;

    // Stream a single file into the archive (non-fatal errors become warnings)
    auto addFileStreaming = [&](const QString& relativePath) {
        const QString relativeNormalizedPath = cwzip_normalizeRelPath(relativePath);
        const QString absolutePath = sourceDir.absoluteFilePath(relativePath);

        QFile file(absolutePath);
        if (!file.open(QIODevice::ReadOnly)) {
            warnings << QStringLiteral("Failed to open file for reading: %1").arg(absolutePath);
            return;
        }

        mz_zip_file fileInfo = {};
        const QByteArray relUtf8 = relativeNormalizedPath.toUtf8();
        // fileInfo.version_madeby = MZ_VERSION_MADEBY;
        fileInfo.compression_method = MZ_COMPRESS_METHOD_DEFLATE; // or MZ_COMPRESS_METHOD_STORE
        fileInfo.filename = relUtf8.constData();
        fileInfo.flag = MZ_ZIP_FLAG_UTF8;
        fileInfo.modified_date = static_cast<uint32_t>(time(nullptr));

        const int32_t openEntryStatus = mz_zip_writer_entry_open(writer.handle, &fileInfo);
        if (openEntryStatus != MZ_OK) {
            warnings << QStringLiteral("Failed to open ZIP entry \"%1\": %2")
            .arg(relativeNormalizedPath, toErrorText(openEntryStatus));
            return;
        }

        constexpr int bufferSize = 128 * 1024;
        QByteArray buffer(bufferSize, Qt::Uninitialized);

        bool hadWriteProblem = false;

        while (true) {
            const qint64 readNow = file.read(buffer.data(), buffer.size());
            if (readNow > 0) {
                const int32_t wrote = mz_zip_writer_entry_write(
                    writer.handle, buffer.constData(), static_cast<int32_t>(readNow));
                if (wrote != readNow) {
                    warnings << QStringLiteral("Short write to ZIP entry \"%1\"").arg(relativeNormalizedPath);
                    hadWriteProblem = true;
                    break;
                }
            } else if (readNow == 0) {
                // End of file
                break;
            } else {
                warnings << QStringLiteral("Error reading file: %1").arg(absolutePath);
                hadWriteProblem = true;
                break;
            }
        }

        const int32_t closeEntryStatus = mz_zip_writer_entry_close(writer.handle);
        if (closeEntryStatus != MZ_OK || hadWriteProblem) {
            warnings << QStringLiteral("Failed to close ZIP entry \"%1\"%2")
            .arg(relativeNormalizedPath,
                 closeEntryStatus == MZ_OK ? QString() :
                     QStringLiteral(": %1").arg(toErrorText(closeEntryStatus)));
        }
    };

    // Recurse directory tree; add files
    std::function<void(const QString&, const QString&)> addDirRecursive;
    addDirRecursive = [&](const QString& dirPath, const QString& relPath) {
        QDir dir(dirPath);
        const QFileInfoList entries = dir.entryInfoList(
            QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);

        for (const QFileInfo& entry : entries) {
            const QString relChild = relPath.isEmpty()
            ? entry.fileName()
            : relPath + "/" + entry.fileName();

            if (entry.isDir()) {
                addDirRecursive(entry.absoluteFilePath(), relChild);
            } else if (entry.isFile()) {
                addFileStreaming(relChild);
            } else {
                // Skip symlinks/special files. Add handling here if you want to include them.
                continue;
            }
        }
    };

    addDirRecursive(sourceDirPath, QString());

    // Finalize archive (fatal if this fails)
    const int32_t closeStatus = mz_zip_writer_close(writer.handle);
    writer.opened = false; // prevent double-close in dtor
    if (closeStatus != MZ_OK) {
        return Monad::ResultBase(
            QStringLiteral("Failed to finalize zip \"%1\": %2")
                .arg(zipFilePath, toErrorText(closeStatus)),
            closeStatus
            );
    }

    // Writer handle deletion handled by RAII ~Writer()

    if (!warnings.isEmpty()) {
        // Aggregate into a single warning string
        const QString joined = warnings.join(QStringLiteral("\n"));
        return Monad::ResultBase(
            QStringLiteral("Created \"%1\" with warnings:\n%2").arg(zipFilePath, joined),
            Monad::ResultBase::Warning
            );
    }

    // Success
    return Monad::ResultBase(); // NoError
}
