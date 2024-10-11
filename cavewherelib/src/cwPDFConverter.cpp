//Our includes
#include "cwPDFConverter.h"
#include "cwDebug.h"

//AsyncFuture
#include "asyncfuture.h"

//Qt includes
#ifdef WITH_PDF_SUPPORT
#include <QPdfDocument>
#endif

cwPDFConverter::cwPDFConverter(QObject *parent) : QObject(parent)
{

}

/**
* The path to the pdf
*/
void cwPDFConverter::setSource(QString source) {
    if(Source != source) {
        Source = source;
        emit sourceChanged();
    }
}

/**
* Sets the resolution of the export pdf. This is in ppi (like in adobe)
* To get 1 to 1 resolution set this to 72. The default is 300ppi (high resolution)
*/
void cwPDFConverter::setResolution(int resolution) {
    if(Resolution != resolution) {
        Resolution = resolution;
        emit resolutionChanged();
    }
}

/**
 * Converts each page of the pdf using QtConcurrent::mapped()
 */
QFuture<QImage> cwPDFConverter::convert()
{
#ifdef WITH_PDF_SUPPORT
    QPdfDocument document;
    auto error = document.load(source());

    if(error != QPdfDocument::NoError) {
        auto toString = [](QPdfDocument::DocumentError error) {
            switch(error) {
            case QPdfDocument::NoError:
                return QStringLiteral("No Error");
            case QPdfDocument::UnknownError:
                return QStringLiteral("Unknown Error :(");
            case QPdfDocument::DataNotYetAvailableError:
                return QStringLiteral("Data Not yet available error");
            case QPdfDocument::FileNotFoundError:
                return QStringLiteral("File not found");
            case QPdfDocument::InvalidFileFormatError:
                return QStringLiteral("Invalid file format");
            case QPdfDocument::IncorrectPasswordError:
                return QStringLiteral("Incorrect password");
            case QPdfDocument::UnsupportedSecuritySchemeError:
                return QStringLiteral("Unsupported Security scheme");
            default:
                return QStringLiteral("Unhandled error, this is a bug :(");
            }
        };

        setError("PDF Renderer error: " + toString(error));
        return QFuture<QImage>();
    }

    QVector<int> pages(document.pageCount());
    std::iota(pages.begin(), pages.end(), 0);

    QString path = source();
    int resolution = this->resolution();

    std::function<QImage (int page)> convertPage = [path, resolution](int page)->QImage {
        QPdfDocument doc;
        doc.load(path);
        auto size = doc.pageSize(page);

        auto scaledSize = size * 1/72.0 * resolution;
        QImage image = doc.render(page, QSize(std::round(scaledSize.width()),
                                              std::round(scaledSize.height())));
        return image;
    };

    return QtConcurrent::mapped(pages, convertPage);
#else
    setError("PDF Renderer not enabled");
    return QFuture<QImage>();
#endif
}

void cwPDFConverter::setError(const QString &errorMessage)
{
    if(ErrorMessage != errorMessage) {
        ErrorMessage = errorMessage;
        emit errorChanged();
    }
}

/**
* Returns true if the converter will work, and falso if it doesn't
*/
bool cwPDFConverter::isSupported()  {
#ifdef WITH_PDF_SUPPORT
    return true;
#else
    return false;
#endif
}
