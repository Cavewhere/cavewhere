#pragma once

//Qt includes
#include <QString>

//Monad
#include <Monad/Result.h>


/**
 * @brief The cwZip class
 *
 * Provides static methods to extract or create ZIP archives using minizip-ng.
 * Only depends on Qt types in the public interface.
 */
class cwZip
{
public:
    struct ZipResult {
        int filesExtracted = 0;
        int directoriesCreated = 0;
        bool hadErrors = false;
    };

    static Monad::Result<ZipResult> extractAll(const QString& zipFilePath, const QString& outputDirectory);
    static Monad::ResultBase zipDirectory(const QString& sourceDirPath, const QString& zipFilePath);

private:
    static bool ensureParentDirectory(const QString& fullPath);
};
