//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwShot.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSaveLoad.h"
#include "cwRootData.h"
#include "cwDiff.h"
#include "cwJobSettings.h"
#include "cwPDFSettings.h"
#include "cwProject.h"
#include "cwSurveyChunk.h"
#include "cwSurveyNoteModel.h"
#include "cwSurveyNoteLiDARModel.h"
#include "cwNote.h"
#include "cwNoteLiDAR.h"
#include "cwImageUtils.h"
#include "cwUnits.h"
#include "cwTeam.h"
#include "cwTeamMember.h"
#include "cwSvgReader.h"
#include "cwRegionIOTask.h"
#include "cavewhereVersion.h"

//Qt includes
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#ifdef CW_WITH_PDF_SUPPORT
#include <QPdfDocument>
#endif
#include <QHash>
#include <QTemporaryDir>

#include <set>

//Test includes
#include "TestHelper.h"

//Protobuf includes
#include <google/protobuf/message.h>
#include <google/protobuf/util/message_differencer.h>
#include <google/protobuf/util/json_util.h>
#include "cavewhere.pb.h"
using namespace google::protobuf;

QDir testDir() {
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QDir dir(desktopPath);
    dir.mkdir("trip");
    dir.cd("trip");
    return dir;
}

template<typename ProtoT>
static ProtoT loadProtoFromJsonFile(const QString& filename) {
    QFile file(filename);
    REQUIRE(file.open(QIODevice::ReadOnly));
    const QByteArray data = file.readAll();
    file.close();

    ProtoT proto;
    const auto status = google::protobuf::util::JsonStringToMessage(data.toStdString(), &proto);
    REQUIRE(status.ok());
    return proto;
}

TEST_CASE("cwSaveLoad saves pdf notes with unique filenames", "[cwSaveLoad]") {
    if (!cwProject::supportedImageFormats().contains("pdf")) {
        return;
    }

    cwPDFSettings::initialize();

    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    cave->addTrip();
    auto trip = cave->trip(0);

    const QString pdfPath = copyToTempFolder("://datasets/test_cwPDFConverter/2page-test.pdf");
    trip->notes()->addFromFiles({QUrl::fromLocalFile(pdfPath)});
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    const QDir notesDir = cwSaveLoad::dir(trip->notes());
    REQUIRE(notesDir.exists());
    const QStringList noteFiles = notesDir.entryList(QStringList() << "*.cwnote", QDir::Files);
    REQUIRE(noteFiles.size() == 2);
}

TEST_CASE("cwSaveLoad writes file version metadata for saved files", "[cwSaveLoad]") {
    auto root = std::make_unique<cwRootData>();
    auto project = root->project();
    auto region = project->cavingRegion();

    region->addCave();
    auto cave = region->cave(0);
    REQUIRE(cave != nullptr);
    cave->addTrip();

    auto trip = cave->trip(0);
    REQUIRE(trip != nullptr);
    const QString noteImagePath = copyToTempFolder("://datasets/test_cwNote/testpage.png");
    trip->notes()->addFromFiles({QUrl::fromLocalFile(noteImagePath)});

    const QString glbPath = copyToTempFolder("://datasets/test_cwSurveyNotesConcatModel/bones.glb");
    trip->notesLiDAR()->addFromFiles({QUrl::fromLocalFile(glbPath)});

    root->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    auto note = trip->notes()->notes().value(0);
    REQUIRE(note != nullptr);

    auto noteLidar = qobject_cast<cwNoteLiDAR*>(trip->notesLiDAR()->notes().value(0));
    REQUIRE(noteLidar != nullptr);

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectPath = QDir(tempDir.path()).filePath(QStringLiteral("version-test.cw"));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();

    auto checkFileVersion = [](const auto& proto) {
        REQUIRE(proto.has_fileversion());
        CHECK(proto.fileversion().version() == cwRegionIOTask::protoVersion());
        CHECK(proto.fileversion().cavewhereversion() == CavewhereVersion.toStdString());
    };

    const QString caveFile = cwSaveLoad::absolutePath(cave);
    const QString tripFile = cwSaveLoad::absolutePath(trip);
    const QString noteFile = cwSaveLoad::absolutePath(note);
    const QString noteLidarFile = cwSaveLoad::absolutePath(noteLidar);

    checkFileVersion(loadProtoFromJsonFile<CavewhereProto::Cave>(caveFile));
    checkFileVersion(loadProtoFromJsonFile<CavewhereProto::Trip>(tripFile));
    checkFileVersion(loadProtoFromJsonFile<CavewhereProto::Note>(noteFile));
    checkFileVersion(loadProtoFromJsonFile<CavewhereProto::NoteLiDAR>(noteLidarFile));
}

TEST_CASE("cwSaveLoad reloads missing image metadata", "[cwSaveLoad]") {
    cwPDFSettings::initialize();
    auto settings = cwPDFSettings::instance();
    const int originalResolution = settings->resolutionImport();
    settings->setResolutionImport(144);

    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    cave->addTrip();
    auto trip = cave->trip(0);

    const QString pngPath = copyToTempFolder("://datasets/test_cwNote/testpage.png");
    const QString svgPath = copyToTempFolder("://datasets/test_cwImageProvider/supportedImage.svg");
    const QString pdfPath = copyToTempFolder("://datasets/test_cwPDFConverter/2page-test.pdf");

    QList<QUrl> noteFiles{
        QUrl::fromLocalFile(pngPath),
        QUrl::fromLocalFile(svgPath)
    };

    const bool pdfSupported = cwProject::supportedImageFormats().contains("pdf");
    if (pdfSupported) {
        noteFiles.append(QUrl::fromLocalFile(pdfPath));
    }

    trip->notes()->addFromFiles(noteFiles);
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    auto clearImageFields = [](const QString& noteFilename) {
        QFile file(noteFilename);
        REQUIRE(file.open(QIODevice::ReadOnly));
        const QByteArray data = file.readAll();
        file.close();

        CavewhereProto::Note proto;
        const auto status = google::protobuf::util::JsonStringToMessage(data.toStdString(), &proto);
        REQUIRE(status.ok());
        proto.mutable_image()->clear_size();
        proto.mutable_image()->clear_dotpermeter();
        proto.mutable_image()->clear_imageunit();

        std::string output;
        google::protobuf::util::JsonPrintOptions options;
        options.add_whitespace = true;
        const auto writeStatus = google::protobuf::util::MessageToJsonString(proto, &output, options);
        REQUIRE(writeStatus.ok());
        REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        REQUIRE(file.write(output.data(), static_cast<qint64>(output.size())) == static_cast<qint64>(output.size()));
        file.close();
    };

    for (cwNote* note : trip->notes()->notes()) {
        clearImageFields(cwSaveLoad::absolutePath(note));
    }

    auto reloaded = std::make_unique<cwProject>();
    addTokenManager(reloaded.get());
    reloaded->loadOrConvert(project->filename());
    reloaded->waitLoadToFinish();

    cwTrip* const loadedTrip = reloaded->cavingRegion()->cave(0)->trip(0);
    REQUIRE(loadedTrip != nullptr);

    auto expectedImageInfo = [settings](const cwNote* note, const QString& imagePath) {
        cwImage::OriginalImageInfo info;
        const QString suffix = QFileInfo(imagePath).suffix().toLower();
        if (suffix == QStringLiteral("pdf")) {
#ifdef CW_WITH_PDF_SUPPORT
            QPdfDocument document;
            if (document.load(imagePath) == QPdfDocument::Error::None) {
                const int pageIndex = note->image().page() >= 0 ? note->image().page() : 0;
                const QSizeF pagePoints = document.pagePointSize(pageIndex);
                const QSize pointSize(qRound(pagePoints.width()),
                                      qRound(pagePoints.height()));
                constexpr double inchesToMeters = cwUnits::convert(1.0, cwUnits::Inches, cwUnits::Meters);
                constexpr double pointsToInches = 1.0 / 72.0;
                constexpr int pointsPerMeter = qRound(1.0 / (pointsToInches * inchesToMeters));
                info.originalSize = pointSize;
                info.originalDotsPerMeter = pointsPerMeter;
                info.unit = cwImage::Unit::Points;
            }
#endif
        } else {
            QFile imageFile(imagePath);
            if (imageFile.open(QIODevice::ReadOnly)) {
                QByteArray imageData = imageFile.readAll();
                const QByteArray format = QFileInfo(imagePath).suffix().toLatin1();
                if (suffix == QStringLiteral("svg")) {
                    info = cwSvgReader::imageInfo(imageData, format);
                } else {
                    QImage image = cwImageUtils::imageWithAutoTransform(imageData, format);
                    info.originalSize = image.size();
                    info.originalDotsPerMeter = image.dotsPerMeterX();
                    info.unit = cwImage::Unit::Pixels;
                }
            }
        }
        return info;
    };

    for (cwNote* note : loadedTrip->notes()->notes()) {
        const QString absolutePath = cwSaveLoad::absolutePath(note, note->image().path());
        const QString suffix = QFileInfo(absolutePath).suffix().toLower();
        const cwImage::OriginalImageInfo expected = expectedImageInfo(note, absolutePath);
        if (suffix == QStringLiteral("pdf") && !expected.originalSize.isValid()) {
            CHECK(note->image().unit() == cwImage::Unit::Points);
            CHECK(note->image().originalSize().isValid());
            CHECK(note->image().originalDotsPerMeter() > 0);
            continue;
        }
        CHECK(note->image().unit() == expected.unit);
        CHECK(note->image().originalSize() == expected.originalSize);
        CHECK(note->image().originalDotsPerMeter() == expected.originalDotsPerMeter);
    }

    settings->setResolutionImport(originalResolution);
}

TEST_CASE("cwSaveLoad preserves cwImage units for png, svg, and pdf notes", "[cwSaveLoad]") {
    cwPDFSettings::initialize();

    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    cave->addTrip();
    auto trip = cave->trip(0);

    const QString pngPath = copyToTempFolder("://datasets/test_cwNote/testpage.png");
    const QString svgPath = copyToTempFolder("://datasets/test_cwImageProvider/supportedImage.svg");
    const QString pdfPath = copyToTempFolder("://datasets/test_cwPDFConverter/2page-test.pdf");

    QList<QUrl> noteFiles{
        QUrl::fromLocalFile(pngPath),
        QUrl::fromLocalFile(svgPath)
    };

    const bool pdfSupported = cwProject::supportedImageFormats().contains("pdf");
    if (pdfSupported) {
        noteFiles.append(QUrl::fromLocalFile(pdfPath));
    }

    trip->notes()->addFromFiles(noteFiles);
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    const int expectedNoteCount = pdfSupported ? 4 : 2;
    REQUIRE(trip->notes()->notes().size() == expectedNoteCount);

    auto expectedUnitForPath = [](const QString& path) {
        const QString suffix = QFileInfo(path).suffix().toLower();
        if (suffix == QStringLiteral("pdf")) {
            return cwImage::Unit::Points;
        }
        if (suffix == QStringLiteral("svg")) {
            return cwImage::Unit::SvgUnits;
        }
        return cwImage::Unit::Pixels;
    };

    struct ExpectedImageData {
        cwImage::Unit expectedUnit;
        QSize originalSize;
        int dotsPerMeter;
        int page;
    };

    auto makeKey = [](const cwImage& image) {
        return QStringLiteral("%1#%2").arg(image.path()).arg(image.page());
    };

    QHash<QString, ExpectedImageData> expectedImages;
    for (const cwNote* note : trip->notes()->notes()) {
        const cwImage image = note->image();
        expectedImages.insert(makeKey(image),
                              ExpectedImageData{
                                  expectedUnitForPath(image.path()),
                                  image.originalSize(),
                                  image.originalDotsPerMeter(),
                                  image.page()
                              });
    }

    const QString projectFilename = project->filename();
    REQUIRE(!projectFilename.isEmpty());

    auto reloaded = std::make_unique<cwProject>();
    addTokenManager(reloaded.get());
    reloaded->loadOrConvert(projectFilename);
    reloaded->waitLoadToFinish();

    cwTrip* const loadedTrip = reloaded->cavingRegion()->cave(0)->trip(0);
    REQUIRE(loadedTrip != nullptr);
    const QList<cwNote*> loadedNotes = loadedTrip->notes()->notes();
    REQUIRE(loadedNotes.size() == expectedImages.size());

    for (const cwNote* note : loadedNotes) {
        const cwImage image = note->image();
        const QString key = makeKey(image);
        INFO("Missing image key: " << key);
        REQUIRE(expectedImages.contains(key));
        const ExpectedImageData expected = expectedImages.value(key);
        CHECK(image.unit() == expected.expectedUnit);
        CHECK(image.originalSize() == expected.originalSize);
        CHECK(image.originalDotsPerMeter() == expected.dotsPerMeter);
        CHECK(image.page() == expected.page);
    }
}


// deep‑compare a & b, but skip ANY field whose descriptor
// appears in excludedFields
bool compareMessagesIgnoring(
    const Message& a,
    const Message& b,
    const std::set<const FieldDescriptor*>& excludedFields)
{
    // same object?
    if (&a == &b) {
        return true;
    }

    // byte‑for‑byte equal?
    if (util::MessageDifferencer::Equals(a, b)) {
        return true;
    }

    // must be same type
    const Descriptor* desc = a.GetDescriptor();
    if (desc != b.GetDescriptor()) {
        return false;
    }

    const Reflection* reflA = a.GetReflection();
    const Reflection* reflB = b.GetReflection();

    // iterate every field in the descriptor
    for (int i = 0; i < desc->field_count(); ++i) {
        const FieldDescriptor* field = desc->field(i);
        // qDebug() << "Field:" << i << field->DebugString();

        // skip this field entirely if asked
        if (excludedFields.count(field)) {
            // qDebug() << "\t excluded!";
            continue;
        }

        // handle repeated fields
        if (field->is_repeated()) {
            int sizeA = reflA->FieldSize(a, field);
            int sizeB = reflB->FieldSize(b, field);
            if (sizeA != sizeB) {
                // qDebug() << "\tRepeated is different sizes!";
                return false;
            }

            for (int idx = 0; idx < sizeA; ++idx) {
                if (field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) {
                    const Message& subA = reflA->GetRepeatedMessage(a, field, idx);
                    const Message& subB = reflB->GetRepeatedMessage(b, field, idx);
                    // qDebug() << "\tSub message!";
                    if (!compareMessagesIgnoring(subA, subB, excludedFields)) {
                        return false;
                    }
                }
                else {
                    // primitive or enum or string
                    switch (field->cpp_type()) {
                    case FieldDescriptor::CPPTYPE_INT32:
                        if (reflA->GetRepeatedInt32(a, field, idx)
                            != reflB->GetRepeatedInt32(b, field, idx)) {
                            return false;
                        }
                        break;
                    case FieldDescriptor::CPPTYPE_INT64:
                        if (reflA->GetRepeatedInt64(a, field, idx)
                            != reflB->GetRepeatedInt64(b, field, idx)) {
                            return false;
                        }
                        break;
                    case FieldDescriptor::CPPTYPE_UINT32:
                        if (reflA->GetRepeatedUInt32(a, field, idx)
                            != reflB->GetRepeatedUInt32(b, field, idx)) {
                            return false;
                        }
                        break;
                    case FieldDescriptor::CPPTYPE_UINT64:
                        if (reflA->GetRepeatedUInt64(a, field, idx)
                            != reflB->GetRepeatedUInt64(b, field, idx)) {
                            return false;
                        }
                        break;
                    case FieldDescriptor::CPPTYPE_DOUBLE:
                        if (reflA->GetRepeatedDouble(a, field, idx)
                            != reflB->GetRepeatedDouble(b, field, idx)) {
                            return false;
                        }
                        break;
                    case FieldDescriptor::CPPTYPE_FLOAT:
                        if (reflA->GetRepeatedFloat(a, field, idx)
                            != reflB->GetRepeatedFloat(b, field, idx)) {
                            return false;
                        }
                        break;
                    case FieldDescriptor::CPPTYPE_BOOL:
                        if (reflA->GetRepeatedBool(a, field, idx)
                            != reflB->GetRepeatedBool(b, field, idx)) {
                            return false;
                        }
                        break;
                    case FieldDescriptor::CPPTYPE_ENUM:
                        if (reflA->GetRepeatedEnumValue(a, field, idx)
                            != reflB->GetRepeatedEnumValue(b, field, idx)) {
                            return false;
                        }
                        break;
                    case FieldDescriptor::CPPTYPE_STRING:
                        if (reflA->GetRepeatedString(a, field, idx)
                            != reflB->GetRepeatedString(b, field, idx)) {
                            return false;
                        }
                        break;
                    default:
                        return false;  // unknown type
                    }
                }
            }
        }
        // singular field
        else {
            bool hasA = reflA->HasField(a, field);
            bool hasB = reflB->HasField(b, field);
            if (hasA != hasB) return false;
            if (!hasA) continue;  // both unset → OK

            if (field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) {
                const Message& subA = reflA->GetMessage(a, field);
                const Message& subB = reflB->GetMessage(b, field);
                if (!compareMessagesIgnoring(subA, subB, excludedFields)) {
                    return false;
                }
            }
            else {
                switch (field->cpp_type()) {
                case FieldDescriptor::CPPTYPE_INT32:
                    if (reflA->GetInt32(a, field)
                        != reflB->GetInt32(b, field)) {
                        return false;
                    }
                    break;
                case FieldDescriptor::CPPTYPE_INT64:
                    if (reflA->GetInt64(a, field)
                        != reflB->GetInt64(b, field)) {
                        return false;
                    }
                    break;
                case FieldDescriptor::CPPTYPE_UINT32:
                    if (reflA->GetUInt32(a, field)
                        != reflB->GetUInt32(b, field)) {
                        return false;
                    }
                    break;
                case FieldDescriptor::CPPTYPE_UINT64:
                    if (reflA->GetUInt64(a, field)
                        != reflB->GetUInt64(b, field)) {
                        return false;
                    }
                    break;
                case FieldDescriptor::CPPTYPE_DOUBLE:
                    if (reflA->GetDouble(a, field)
                        != reflB->GetDouble(b, field)) {
                        return false;
                    }
                    break;
                case FieldDescriptor::CPPTYPE_FLOAT:
                    if (reflA->GetFloat(a, field)
                        != reflB->GetFloat(b, field)) {
                        return false;
                    }
                    break;
                case FieldDescriptor::CPPTYPE_BOOL:
                    if (reflA->GetBool(a, field)
                        != reflB->GetBool(b, field)) {
                        return false;
                    }
                    break;
                case FieldDescriptor::CPPTYPE_ENUM:
                    if (reflA->GetEnumValue(a, field)
                        != reflB->GetEnumValue(b, field)) {
                        return false;
                    }
                    break;
                case FieldDescriptor::CPPTYPE_STRING:
                    if (reflA->GetString(a, field)
                        != reflB->GetString(b, field)) {
                        return false;
                    }
                    break;
                default:
                    return false;  // unknown type
                }
            }
        }
    }

    // all fields matched
    return true;
}

template<typename T>
void dumpProto(T base, T ours, T theirs, T merged) {
    QDir dir = testDir();
    // dir.removeRecursively();
    dir = testDir();

    dir.mkdir("ours");
    QDir oursDir = dir;
    oursDir.cd("ours");

    dir.mkdir("theirs");
    QDir theirsDir = dir;
    theirsDir.cd("theirs");

    dir.mkdir("merged");
    QDir mergedDir = dir;
    mergedDir.cd("merged");

    cwSaveLoad save;

    save.saveProtoMessage(dir, "base.json", std::move(base));
    save.saveProtoMessage(oursDir, "ours.json", std::move(ours));
    save.saveProtoMessage(theirsDir, "theirs.json", std::move(theirs));
    save.saveProtoMessage(mergedDir, "merged.json", std::move(merged));

    save.waitForFinished();
}


// Recursively tweak every scalar (singular or repeated) and dive into sub‐messages
static void mutateScalarsRecursively(
    const google::protobuf::Message& baseMsg,
    google::protobuf::Message&      oursMsg,
    google::protobuf::Message&      theirsMsg)
{
    const auto* descriptor = baseMsg.GetDescriptor();
    const auto* reflection = baseMsg.GetReflection();

    for (int i = 0; i < descriptor->field_count(); ++i) {
        const auto* field = descriptor->field(i);

        //Skip id fields
        if(field->name() == "id") {
            continue;
        }

        // ----- repeated fields -----
        if (field->is_repeated()) {
            int count = reflection->FieldSize(baseMsg, field);

            // repeated sub‐messages
            if (field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
                for (int index = 0; index < count; ++index) {
                    const auto& baseSub = reflection->GetRepeatedMessage(baseMsg, field, index);
                    auto* oursSub       = reflection->MutableRepeatedMessage(&oursMsg, field, index);
                    auto* theirsSub     = reflection->MutableRepeatedMessage(&theirsMsg, field, index);
                    mutateScalarsRecursively(baseSub, *oursSub, *theirsSub);
                }
            }
            // repeated scalars
            else {
                for (int index = 0; index < count; ++index) {
                    switch (field->cpp_type()) {
                    case google::protobuf::FieldDescriptor::CPPTYPE_INT32: {
                        int32_t value = reflection->GetRepeatedInt32(baseMsg, field, index);
                        reflection->SetRepeatedInt32(&oursMsg,   field, index, value + 100);
                        reflection->SetRepeatedInt32(&theirsMsg, field, index, value + 200);
                        break;
                    }
                    case google::protobuf::FieldDescriptor::CPPTYPE_INT64: {
                        int64_t value = reflection->GetRepeatedInt64(baseMsg, field, index);
                        reflection->SetRepeatedInt64(&oursMsg,   field, index, value + 100);
                        reflection->SetRepeatedInt64(&theirsMsg, field, index, value + 200);
                        break;
                    }
                    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32: {
                        uint32_t value = reflection->GetRepeatedUInt32(baseMsg, field, index);
                        reflection->SetRepeatedUInt32(&oursMsg,   field, index, value + 100u);
                        reflection->SetRepeatedUInt32(&theirsMsg, field, index, value + 200u);
                        break;
                    }
                    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64: {
                        uint64_t value = reflection->GetRepeatedUInt64(baseMsg, field, index);
                        reflection->SetRepeatedUInt64(&oursMsg,   field, index, value + 100u);
                        reflection->SetRepeatedUInt64(&theirsMsg, field, index, value + 200u);
                        break;
                    }
                    case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE: {
                        double value = reflection->GetRepeatedDouble(baseMsg, field, index);
                        reflection->SetRepeatedDouble(&oursMsg,   field, index, value + 100.0);
                        reflection->SetRepeatedDouble(&theirsMsg, field, index, value + 200.0);
                        break;
                    }
                    case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT: {
                        float value = reflection->GetRepeatedFloat(baseMsg, field, index);
                        reflection->SetRepeatedFloat(&oursMsg,   field, index, value + 100.0f);
                        reflection->SetRepeatedFloat(&theirsMsg, field, index, value + 200.0f);
                        break;
                    }
                    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL: {
                        bool value = reflection->GetRepeatedBool(baseMsg, field, index);
                        reflection->SetRepeatedBool(&oursMsg,   field, index, !value);
                        reflection->SetRepeatedBool(&theirsMsg, field, index, value);
                        break;
                    }
                    case google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
                        std::string value = reflection->GetRepeatedString(baseMsg, field, index);
                        reflection->SetRepeatedString(&oursMsg,   field, index, value + "_ours");
                        reflection->SetRepeatedString(&theirsMsg, field, index, value + "_theirs");
                        break;
                    }
                    default: {
                        // skip enums / bytes / unknown
                        break;
                    }
                    }
                }
            }

            continue;  // done with this field
        }

        // ----- singular message fields -----
        if (field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
            auto* oursSub   = reflection->MutableMessage(&oursMsg,   field);
            auto* theirsSub = reflection->MutableMessage(&theirsMsg, field);
            mutateScalarsRecursively(
                reflection->GetMessage(baseMsg, field),
                *oursSub,
                *theirsSub
                );
            continue;
        }

        // ----- singular scalar fields -----
        switch (field->cpp_type()) {
        case google::protobuf::FieldDescriptor::CPPTYPE_INT32: {
            int32_t value = reflection->GetInt32(baseMsg, field);
            reflection->SetInt32(&oursMsg,   field, value + 100);
            reflection->SetInt32(&theirsMsg, field, value + 200);
            break;
        }
        case google::protobuf::FieldDescriptor::CPPTYPE_INT64: {
            int64_t value = reflection->GetInt64(baseMsg, field);
            reflection->SetInt64(&oursMsg,   field, value + 100);
            reflection->SetInt64(&theirsMsg, field, value + 200);
            break;
        }
        case google::protobuf::FieldDescriptor::CPPTYPE_UINT32: {
            uint32_t value = reflection->GetUInt32(baseMsg, field);
            reflection->SetUInt32(&oursMsg,   field, value + 100u);
            reflection->SetUInt32(&theirsMsg, field, value + 200u);
            break;
        }
        case google::protobuf::FieldDescriptor::CPPTYPE_UINT64: {
            uint64_t value = reflection->GetUInt64(baseMsg, field);
            reflection->SetUInt64(&oursMsg,   field, value + 100u);
            reflection->SetUInt64(&theirsMsg, field, value + 200u);
            break;
        }
        case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE: {
            double value = reflection->GetDouble(baseMsg, field);
            reflection->SetDouble(&oursMsg,   field, value + 100.0);
            reflection->SetDouble(&theirsMsg, field, value + 200.0);
            break;
        }
        case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT: {
            float value = reflection->GetFloat(baseMsg, field);
            reflection->SetFloat(&oursMsg,   field, value + 100.0f);
            reflection->SetFloat(&theirsMsg, field, value + 200.0f);
            break;
        }
        case google::protobuf::FieldDescriptor::CPPTYPE_BOOL: {
            bool value = reflection->GetBool(baseMsg, field);
            reflection->SetBool(&oursMsg,   field, !value);
            reflection->SetBool(&theirsMsg, field, !value);
            break;
        }
        case google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
            std::string value = reflection->GetString(baseMsg, field);
            reflection->SetString(&oursMsg,   field, value + "_ours");
            reflection->SetString(&theirsMsg, field, value + "_theirs");
            break;
        }
        default: {
            // skip enums / bytes / unknown
            break;
        }
        }
    }
}

TEST_CASE("Test the sanitized for directory name", "[cwSaveLoad]") {
    SECTION("Replaces forbidden characters") {
        QString input = R"(My:Invalid/Name|Test)";
        QString expected = "My_Invalid_Name_Test";
        REQUIRE(cwSaveLoad::sanitizeFileName(input).toStdString() == expected.toStdString());
    }

    SECTION("Trims whitespace") {
        QString input = "   directory name   ";
        QString expected = "directory name";
        REQUIRE(cwSaveLoad::sanitizeFileName(input) == expected);
    }

    SECTION("Removes leading and trailing dots") {
        QString input = "..hidden.name..";
        QString expected = "hidden.name";
        REQUIRE(cwSaveLoad::sanitizeFileName(input) == expected);
    }

    SECTION("Handles all forbidden characters") {
        QString input = R"(\/:*?"<>|)";
        QString expected = "_________";
        REQUIRE(cwSaveLoad::sanitizeFileName(input) == expected);
    }

    SECTION("Returns 'untitled' when input is empty") {
        REQUIRE(cwSaveLoad::sanitizeFileName("") == "untitled");
    }

    SECTION("Returns 'untitled' when result becomes empty") {
        QString input = "......";
        REQUIRE(cwSaveLoad::sanitizeFileName(input) == "untitled");
    }

    SECTION("Valid name remains unchanged") {
        QString input = "Valid_Directory_Name";
        REQUIRE(cwSaveLoad::sanitizeFileName(input) == input);
    }
}

TEST_CASE("cwSaveLoad should save and load cwTrip - empty", "[cwSaveLoad]") {
    auto dir = testDir();

    cwTrip emptyTrip;

    cwSaveLoad save;
    save.save(dir, &emptyTrip);

    save.waitForFinished();
}

TEST_CASE("cwSaveLoad should save and load cwTrip - complex", "[cwSaveLoad]") {
    auto root = std::make_unique<cwRootData>();
    auto filename = copyToTempFolder("://datasets/test_cwProject/Phake Cave 3000.cw");

    root->project()->loadOrConvert(filename);
    root->project()->waitLoadToFinish();

    REQUIRE(root->project()->cavingRegion()->caveCount() >= 1);
    auto cave = root->project()->cavingRegion()->cave(0);

    REQUIRE(cave->tripCount() >= 1);
    auto trip = cave->trip(0);
    REQUIRE(trip != nullptr);

    cwSaveLoad save;
    auto dir = testDir();

    save.save(dir, trip);
    save.waitForFinished();

}

TEST_CASE("cwSaveLoad should save and load old projects correctly", "[cwSaveLoad]") {
    auto root = std::make_unique<cwRootData>();
    auto filename = copyToTempFolder("://datasets/test_cwProject/Phake Cave 3000.cw");
    // auto filename = "/Users/cave/Desktop/BlankenshipBlowhole.cw";

    //Prevents loop closure from happening
    root->settings()->jobSettings()->setAutomaticUpdate(false);

    root->project()->loadOrConvert(filename);
    root->project()->waitLoadToFinish();
    root->futureManagerModel()->waitForFinished();
    root->project()->waitSaveToFinish();

    REQUIRE(root->project()->cavingRegion()->caveCount() >= 1);
    auto cave = root->project()->cavingRegion()->cave(0);

    REQUIRE(cave->tripCount() >= 1);
    auto trip = cave->trip(0);
    REQUIRE(trip != nullptr);

    auto root2 = std::make_unique<cwRootData>();

    CHECK(root->project()->projectType(filename) == cwProject::SqliteFileType);

    QString convertedFilename = root->project()->filename();
    REQUIRE(!convertedFilename.isEmpty());
    CHECK(QFileInfo::exists(convertedFilename));
    CHECK(root->project()->projectType(convertedFilename) == cwProject::GitFileType);

    root2->settings()->jobSettings()->setAutomaticUpdate(false);
    root2->project()->loadFile(convertedFilename);
    root2->project()->waitLoadToFinish();
    root2->futureManagerModel()->waitForFinished();
    root2->project()->waitSaveToFinish();

    REQUIRE(root2->project()->cavingRegion()->caveCount() >= 1);

    auto region1 = root->project()->cavingRegion();
    auto region2 = root2->project()->cavingRegion();

    REQUIRE(region1->caveCount() == region2->caveCount());

    auto compareNoteImages = [](const cwSurveyNoteModel* expected, const cwSurveyNoteModel* actual) {
        REQUIRE(expected->notes().size() == actual->notes().size());
        for (int i = 0; i < expected->notes().size(); ++i) {
            const auto* expectedNote = expected->notes().at(i);
            const auto* actualNote = actual->notes().at(i);

            REQUIRE(actualNote != nullptr);

            CHECK(expectedNote->name() == actualNote->name());
            CHECK(qFuzzyCompare(expectedNote->rotate(), actualNote->rotate()));
            CHECK(expectedNote->image().mode() == actualNote->image().mode());
            CHECK(expectedNote->image().path() == actualNote->image().path());
        }
    };

    auto compareNoteLiDAR = [](const cwSurveyNoteLiDARModel* expected, const cwSurveyNoteLiDARModel* actual) {
        const auto expectedNotes = expected->notes();
        const auto actualNotes = actual->notes();

        REQUIRE(expectedNotes.size() == actualNotes.size());

        for (int i = 0; i < expectedNotes.size(); ++i) {
            auto* expectedNote = qobject_cast<cwNoteLiDAR*>(expectedNotes.at(i));
            auto* actualNote = qobject_cast<cwNoteLiDAR*>(actualNotes.at(i));

            REQUIRE(expectedNote != nullptr);
            REQUIRE(actualNote != nullptr);

            CHECK(expectedNote->name() == actualNote->name());
            CHECK(expectedNote->filename() == actualNote->filename());
            CHECK(expectedNote->autoCalculateNorth() == actualNote->autoCalculateNorth());
            CHECK(expectedNote->stations().size() == actualNote->stations().size());
        }
    };

    auto compareTrips = [&](const cwTrip* expectedTrip, const cwTrip* actualTrip) {
        REQUIRE(actualTrip != nullptr);
        CHECK(expectedTrip->name() == actualTrip->name());
        CHECK(expectedTrip->date() == actualTrip->date());
        CHECK(expectedTrip->chunks().size() == actualTrip->chunks().size());

        auto expectedProto = cwSaveLoad::toProtoTrip(expectedTrip);
        auto actualProto = cwSaveLoad::toProtoTrip(actualTrip);
        std::set<const FieldDescriptor*> ignoredFields;
        CHECK(compareMessagesIgnoring(*expectedProto, *actualProto, ignoredFields));

        compareNoteImages(expectedTrip->notes(), actualTrip->notes());
        compareNoteLiDAR(expectedTrip->notesLiDAR(), actualTrip->notesLiDAR());

        auto compareTeams = [](cwTeam* expected, cwTeam* actual) {
            REQUIRE(actual != nullptr);

            const auto expectedMembers = expected->teamMembers();
            const auto actualMembers = actual->teamMembers();

            REQUIRE(expectedMembers.size() == actualMembers.size());

            for (int i = 0; i < expectedMembers.size(); ++i) {
                const auto& expectedMember = expectedMembers.at(i);
                const auto& actualMember = actualMembers.at(i);

                CHECK(expectedMember.name() == actualMember.name());
                CHECK(expectedMember.jobs() == actualMember.jobs());
            }
        };

        compareTeams(expectedTrip->team(), actualTrip->team());
    };

    for (int caveIndex = 0; caveIndex < region1->caveCount(); ++caveIndex) {
        auto cave1 = region1->cave(caveIndex);
        auto cave2 = region2->cave(caveIndex);

        REQUIRE(cave1 != nullptr);
        REQUIRE(cave2 != nullptr);

        CHECK(cave1->name() == cave2->name());
        CHECK(cave1->tripCount() == cave2->tripCount());
        checkStationLookup(cave1->stationPositionLookup(), cave2->stationPositionLookup());

        for (int tripIndex = 0; tripIndex < cave1->tripCount(); ++tripIndex) {
            compareTrips(cave1->trip(tripIndex), cave2->trip(tripIndex));
        }
    }
}


TEST_CASE("cwSaveLoad should 3-way merge cwTrip correctly", "[cwSaveLoad]") {
    auto root = std::make_unique<cwRootData>();
    auto filename = copyToTempFolder("://datasets/test_cwProject/Phake Cave 3000.cw");

    //Prevents loop closure from happening
    root->settings()->jobSettings()->setAutomaticUpdate(false);

    root->project()->loadOrConvert(filename);
    root->project()->waitLoadToFinish();

    REQUIRE(root->project()->cavingRegion()->caveCount() >= 1);
    auto cave = root->project()->cavingRegion()->cave(0);

    REQUIRE(cave->tripCount() >= 1);
    auto trip = cave->trip(0);

    auto base = cwSaveLoad::toProtoTrip(trip);

    SECTION("Change the date") {
        SECTION("Ours") {
            trip->setDate(QDateTime(QDate(2025,07,14), QTime()));

            auto change = cwSaveLoad::toProtoTrip(trip);

            const google::protobuf::Message& baseRef = (*base.get());
            const google::protobuf::Message& oursRef = (*change.get());
            const google::protobuf::Message& thiersRef = baseRef;

            auto mergedTrip = cwDiff::mergeMessageByReflection(baseRef,
                                                               oursRef,
                                                               thiersRef,
                                                               cwDiff::MergeStrategy::UseOursOnConflict);
            auto mergedTripPtr = dynamic_cast<CavewhereProto::Trip*>(mergedTrip.get());

            REQUIRE(mergedTripPtr != nullptr);

            CHECK(!google::protobuf::util::MessageDifferencer::Equals(base->date(), mergedTripPtr->date()));
            CHECK(google::protobuf::util::MessageDifferencer::Equals(change->date(), mergedTripPtr->date()));
        }

        SECTION("Thiers") {
            trip->setDate(QDateTime(QDate(2025,07,14), QTime()));

            auto change = cwSaveLoad::toProtoTrip(trip);

            const google::protobuf::Message& baseRef = (*base.get());
            const google::protobuf::Message& oursRef = baseRef;
            const google::protobuf::Message& thiersRef = (*change.get());

            auto mergedTrip = cwDiff::mergeMessageByReflection(baseRef,
                                                               oursRef,
                                                               thiersRef,
                                                               cwDiff::MergeStrategy::UseTheirsOnConflict);
            auto mergedTripPtr = dynamic_cast<CavewhereProto::Trip*>(mergedTrip.get());

            REQUIRE(mergedTripPtr != nullptr);

            CHECK(!google::protobuf::util::MessageDifferencer::Equals(base->date(), mergedTripPtr->date()));
            CHECK(google::protobuf::util::MessageDifferencer::Equals(change->date(), mergedTripPtr->date()));
        }

        SECTION("Conflict, take ours") {
            trip->setDate(QDateTime(QDate(2025,07,14), QTime()));
            auto ours = cwSaveLoad::toProtoTrip(trip);

            trip->setDate(QDateTime(QDate(2025,07,01), QTime()));
            auto theirs = cwSaveLoad::toProtoTrip(trip);

            const google::protobuf::Message& baseRef = (*base.get());
            const google::protobuf::Message& oursRef = (*ours.get());
            const google::protobuf::Message& thiersRef = (*theirs.get());

            auto mergedTrip = cwDiff::mergeMessageByReflection(baseRef,
                                                               oursRef,
                                                               thiersRef,
                                                               cwDiff::MergeStrategy::UseOursOnConflict);
            auto mergedTripPtr = dynamic_cast<CavewhereProto::Trip*>(mergedTrip.get());

            REQUIRE(mergedTripPtr != nullptr);

            CHECK(google::protobuf::util::MessageDifferencer::Equals(ours->date(), mergedTripPtr->date()));
        }

        SECTION("Conflict, take theirs") {
            trip->setDate(QDateTime(QDate(2025,07,14), QTime()));
            auto ours = cwSaveLoad::toProtoTrip(trip);

            trip->setDate(QDateTime(QDate(2025,07,01), QTime()));
            auto theirs = cwSaveLoad::toProtoTrip(trip);

            const google::protobuf::Message& baseRef = (*base.get());
            const google::protobuf::Message& oursRef = (*ours.get());
            const google::protobuf::Message& thiersRef = (*theirs.get());

            auto mergedTrip = cwDiff::mergeMessageByReflection(baseRef,
                                                               oursRef,
                                                               thiersRef,
                                                               cwDiff::MergeStrategy::UseTheirsOnConflict);
            auto mergedTripPtr = dynamic_cast<CavewhereProto::Trip*>(mergedTrip.get());

            REQUIRE(mergedTripPtr != nullptr);

            CHECK(google::protobuf::util::MessageDifferencer::Equals(theirs->date(), mergedTripPtr->date()));
        }
    }

    SECTION("Change the update the chunk data") {

        SECTION("Add new chunk") {
            cwStation fromStation;
            fromStation.setName("a1");

            cwStation toStation;
            toStation.setName("test1");

            cwShot shot;
            shot.setDistance(cwDistanceReading("20.1"));
            shot.setClino(cwClinoReading("10.2"));
            shot.setCompass(cwCompassReading("7.3"));

            trip->addShotToLastChunk(fromStation, toStation, shot);

            SECTION("Ours") {
                auto change = cwSaveLoad::toProtoTrip(trip);

                const google::protobuf::Message& baseRef = (*base.get());
                const google::protobuf::Message& oursRef = (*change.get());
                const google::protobuf::Message& thiersRef = (*base.get());

                auto mergedTrip = cwDiff::mergeMessageByReflection(baseRef,
                                                                   oursRef,
                                                                   thiersRef,
                                                                   cwDiff::MergeStrategy::UseOursOnConflict);
                auto mergedTripPtr = [&]()->std::unique_ptr<CavewhereProto::Trip> {
                    if(auto raw = dynamic_cast<CavewhereProto::Trip*>(mergedTrip.get())) {
                        mergedTrip.release();
                        return std::unique_ptr<CavewhereProto::Trip>(raw);
                    }
                    return nullptr;
                }();
                REQUIRE(mergedTripPtr != nullptr);

                CHECK(!google::protobuf::util::MessageDifferencer::Equals(*base, *mergedTripPtr));
                CHECK(google::protobuf::util::MessageDifferencer::Equals(*change, *mergedTripPtr));

                //TODO make sure merge changes are real
                REQUIRE(mergedTripPtr->chunks_size() == 5);

                auto newChunk = mergedTripPtr->chunks(4);
                REQUIRE(newChunk.leg_size() == 3);

                int chunksFieldNumber = 5;
                auto chunkField = base->descriptor()->FindFieldByNumber(5);
                CHECK(compareMessagesIgnoring(*base, *mergedTripPtr, {chunkField}));

                REQUIRE(base->chunks_size() == 4);
                for(int i = 0; i < base->chunks_size(); ++i) {
                    auto baseChunk = base->chunks(i);
                    auto mergedChunk = mergedTripPtr->chunks(i);
                    CHECK(google::protobuf::util::MessageDifferencer::Equals(baseChunk, mergedChunk));
                }

                CHECK(newChunk.leg(0).has_name());
                CHECK(newChunk.leg(0).name() == "a1");

                CHECK(newChunk.leg(1).has_distance());
                CHECK(newChunk.leg(1).distance() == "20.1");
                CHECK(newChunk.leg(1).has_clino());
                CHECK(newChunk.leg(1).clino() == "10.2");
                CHECK(newChunk.leg(1).has_compass());
                CHECK(newChunk.leg(1).compass() == "7.3");

                CHECK(newChunk.leg(2).has_name());
                CHECK(newChunk.leg(2).name() == "test1");

                // dumpProto(std::move(base),
                //           std::move(change),
                //           std::unique_ptr<CavewhereProto::Trip>(nullptr),
                //           std::move(mergedTripPtr));

            }

            SECTION("Concat ours / thiers") {
                auto ourChange = cwSaveLoad::toProtoTrip(trip);

                auto lastChunkIndex = trip->chunkCount() - 1;
                trip->removeChunks(lastChunkIndex, lastChunkIndex);

                cwStation fromStation;
                fromStation.setName("a2");

                cwStation toStation;
                toStation.setName("c10");

                cwShot shot;
                shot.setDistance(cwDistanceReading("21.1"));
                shot.setClino(cwClinoReading("11.2"));
                shot.setCompass(cwCompassReading("71.3"));

                trip->addShotToLastChunk(fromStation, toStation, shot);

                auto theirsChange = cwSaveLoad::toProtoTrip(trip);

                const google::protobuf::Message& baseRef = (*base.get());
                const google::protobuf::Message& oursRef = (*ourChange.get());
                const google::protobuf::Message& thiersRef = (*theirsChange.get());

                auto mergedTrip = cwDiff::mergeMessageByReflection(baseRef,
                                                                   oursRef,
                                                                   thiersRef,
                                                                   cwDiff::MergeStrategy::UseOursOnConflict);
                auto mergedTripPtr = [&]()->std::unique_ptr<CavewhereProto::Trip> {
                    if(auto raw = dynamic_cast<CavewhereProto::Trip*>(mergedTrip.get())) {
                        mergedTrip.release();
                        return std::unique_ptr<CavewhereProto::Trip>(raw);
                    }
                    return nullptr;
                }();
                REQUIRE(mergedTripPtr != nullptr);

                CHECK(!google::protobuf::util::MessageDifferencer::Equals(*base, *mergedTripPtr));
                CHECK(!google::protobuf::util::MessageDifferencer::Equals(*ourChange, *mergedTripPtr));
                CHECK(!google::protobuf::util::MessageDifferencer::Equals(*theirsChange, *mergedTripPtr));

                int chunksFieldNumber = 5;
                auto chunkField = base->descriptor()->FindFieldByNumber(5);
                CHECK(compareMessagesIgnoring(*base, *mergedTripPtr, {chunkField}));

                REQUIRE(mergedTripPtr->chunks_size() == 6);
                REQUIRE(base->chunks_size() == 4);
                for(int i = 0; i < base->chunks_size(); ++i) {
                    auto baseChunk = base->chunks(i);
                    auto mergedChunk = mergedTripPtr->chunks(i);
                    CHECK(google::protobuf::util::MessageDifferencer::Equals(baseChunk, mergedChunk));
                }

                auto newChunkOurs = mergedTripPtr->chunks(4);
                REQUIRE(newChunkOurs.leg_size() == 3);

                CHECK(newChunkOurs.leg(0).has_name());
                CHECK(newChunkOurs.leg(0).name() == "a1");

                CHECK(newChunkOurs.leg(1).has_distance());
                CHECK(newChunkOurs.leg(1).distance() == "20.1");
                CHECK(newChunkOurs.leg(1).has_clino());
                CHECK(newChunkOurs.leg(1).clino() == "10.2");
                CHECK(newChunkOurs.leg(1).has_compass());
                CHECK(newChunkOurs.leg(1).compass() == "7.3");

                CHECK(newChunkOurs.leg(2).has_name());
                CHECK(newChunkOurs.leg(2).name() == "test1");

                auto newChunkTheirs = mergedTripPtr->chunks(5);
                REQUIRE(newChunkTheirs.leg_size() == 3);

                CHECK(newChunkTheirs.leg(0).has_name());
                CHECK(newChunkTheirs.leg(0).name() == "a2");

                CHECK(newChunkTheirs.leg(1).has_distance());
                CHECK(newChunkTheirs.leg(1).distance() == "21.1");
                CHECK(newChunkTheirs.leg(1).has_clino());
                CHECK(newChunkTheirs.leg(1).clino() == "11.2");
                CHECK(newChunkTheirs.leg(1).has_compass());
                CHECK(newChunkTheirs.leg(1).compass() == "71.3");

                CHECK(newChunkTheirs.leg(2).has_name());
                CHECK(newChunkTheirs.leg(2).name() == "c10");


                // dumpProto(std::move(base),
                //           std::move(ourChange),
                //           std::move(theirsChange),
                //           std::move(mergedTripPtr));
            }
        }

        SECTION("Change station name and add shot at end") {

            auto ourLastChunk = trip->chunk(trip->chunkCount() - 1);
            auto ourLastStation = ourLastChunk->stationCount() - 2;
            auto baseLastStationName = ourLastChunk->station(ourLastStation).name();
            ourLastChunk->setData(cwSurveyChunk::StationNameRole, ourLastStation, "d1");
            auto ourChange = cwSaveLoad::toProtoTrip(trip);

            //Change it back
            ourLastChunk->setData(cwSurveyChunk::StationNameRole, ourLastStation, baseLastStationName);

            //Thiers
            auto chunk = trip->chunk(trip->chunkCount() - 1);

            auto lastShot = chunk->shotCount() - 1;
            chunk->setData(cwSurveyChunk::ShotDistanceRole, lastShot, "30.2");
            chunk->setData(cwSurveyChunk::ShotCompassRole, lastShot, "123.1");
            chunk->setData(cwSurveyChunk::ShotClinoRole, lastShot, "23.4");

            auto lastStation = chunk->stationCount() - 1;
            chunk->setData(cwSurveyChunk::StationNameRole, lastStation, "c4");

            auto theirsChange = cwSaveLoad::toProtoTrip(trip);

            const google::protobuf::Message& baseRef = (*base.get());
            const google::protobuf::Message& oursRef = (*ourChange.get());
            const google::protobuf::Message& thiersRef = (*theirsChange.get());

            auto mergedTrip = cwDiff::mergeMessageByReflection(baseRef,
                                                               oursRef,
                                                               thiersRef,
                                                               cwDiff::MergeStrategy::UseOursOnConflict);
            auto mergedTripPtr = [&]()->std::unique_ptr<CavewhereProto::Trip> {
                if(auto raw = dynamic_cast<CavewhereProto::Trip*>(mergedTrip.get())) {
                    mergedTrip.release();
                    return std::unique_ptr<CavewhereProto::Trip>(raw);
                }
                return nullptr;
            }();
            REQUIRE(mergedTripPtr != nullptr);

            CHECK(!google::protobuf::util::MessageDifferencer::Equals(*base, *mergedTripPtr));
            CHECK(!google::protobuf::util::MessageDifferencer::Equals(*ourChange, *mergedTripPtr));
            CHECK(!google::protobuf::util::MessageDifferencer::Equals(*theirsChange, *mergedTripPtr));

            int chunksFieldNumber = 5;
            auto chunkField = base->descriptor()->FindFieldByNumber(5);
            CHECK(compareMessagesIgnoring(*base, *mergedTripPtr, {chunkField}));

            REQUIRE(mergedTripPtr->chunks_size() == 4);
            REQUIRE(base->chunks_size() == 4);

            //Skip the last chunk, because that's the one we changed
            for(int i = 0; i < 3; ++i) {
                auto baseChunk = base->chunks(i);
                auto mergedChunk = mergedTripPtr->chunks(i);
                CHECK(google::protobuf::util::MessageDifferencer::Equals(baseChunk, mergedChunk));
            }

            //The leg should be the same exepect for the last 3 entries, station and shot
            const auto& mergedLastChunk = mergedTripPtr->chunks(3);
            const auto& baseLastChunk = base->chunks(3);

            CHECK(!google::protobuf::util::MessageDifferencer::Equals(baseLastChunk, mergedLastChunk));

            //Everything should be the same expect for the legField
            auto legField = baseLastChunk.descriptor()->FindFieldByNumber(4);
            CHECK(compareMessagesIgnoring(baseLastChunk, mergedLastChunk, {legField}));

            REQUIRE(mergedLastChunk.leg_size() == 7);
            REQUIRE(baseLastChunk.leg_size() == 7);

            //All the leg data should be the same expect for the last three rows
            for(int i = 0; i < 4; i++) {
                auto baseStationShot = baseLastChunk.leg(i);
                auto mergedStationShot = mergedLastChunk.leg(i);
                CHECK(util::MessageDifferencer::Equals(baseStationShot, mergedStationShot));
            }

            auto stationRenamed = mergedLastChunk.leg(4);
            CHECK(stationRenamed.name() == "d1");

            auto newShot = mergedLastChunk.leg(5);
            CHECK(newShot.distance() == "30.2");
            CHECK(newShot.compass() == "123.1");
            CHECK(newShot.clino() == "23.4");

            auto newStation = mergedLastChunk.leg(6);
            CHECK(newStation.name() == "c4");

            // dumpProto(std::move(base),
            //           std::move(ourChange),
            //           std::move(theirsChange),
            //           std::move(mergedTripPtr));
        }

        SECTION("Modify the same the same data") {
            REQUIRE(trip->chunkCount() > 0);
            auto firstChunk = trip->chunk(0);
            firstChunk->setData(cwSurveyChunk::StationNameRole, 0, "b1"); //from a1 -> b1

            auto ourChange = cwSaveLoad::toProtoTrip(trip);
            auto theirsChange = cwSaveLoad::toProtoTrip(trip);

            const google::protobuf::Message& baseRef = (*base.get());
            const google::protobuf::Message& oursRef = (*ourChange.get());
            const google::protobuf::Message& thiersRef = (*theirsChange.get());

            auto mergedTrip = cwDiff::mergeMessageByReflection(baseRef,
                                                               oursRef,
                                                               thiersRef,
                                                               cwDiff::MergeStrategy::UseOursOnConflict);
            auto mergedTripPtr = [&]()->std::unique_ptr<CavewhereProto::Trip> {
                if(auto raw = dynamic_cast<CavewhereProto::Trip*>(mergedTrip.get())) {
                    mergedTrip.release();
                    return std::unique_ptr<CavewhereProto::Trip>(raw);
                }
                return nullptr;
            }();
            REQUIRE(mergedTripPtr != nullptr);

            CHECK(!google::protobuf::util::MessageDifferencer::Equals(*base, *mergedTripPtr));
            CHECK(google::protobuf::util::MessageDifferencer::Equals(*ourChange, *mergedTripPtr));
            CHECK(google::protobuf::util::MessageDifferencer::Equals(*theirsChange, *mergedTripPtr));

            int chunksFieldNumber = 5;
            auto chunkField = base->descriptor()->FindFieldByNumber(5);
            CHECK(compareMessagesIgnoring(*base, *mergedTripPtr, {chunkField}));

            REQUIRE(base->chunks_size() == 4);
            REQUIRE(mergedTripPtr->chunks_size() == 4);

            //Check that everything that the chunk are the same
            const auto& baseFirstChunk = base->chunks(0);
            const auto& mergedFirstChunk = mergedTripPtr->chunks(0);
            CHECK(!util::MessageDifferencer::Equals(baseFirstChunk, mergedFirstChunk));

            REQUIRE(baseFirstChunk.leg_size() == 9);
            REQUIRE(mergedFirstChunk.leg_size() == 9);

            const auto& baseFirstStation = baseFirstChunk.leg(0);
            const auto& mergedFirstStation = mergedFirstChunk.leg(0);

            CHECK(baseFirstStation.name() == "a1");
            CHECK(mergedFirstStation.name() == "b1");

            auto stationNameField = mergedFirstStation.descriptor()->FindFieldByNumber(100);
            CHECK(compareMessagesIgnoring(baseFirstStation, mergedFirstStation, {stationNameField}));

            //Make sure all the other leg data in the first chunk is the same
            for(int i = 1; i < baseFirstChunk.leg_size(); i++) {
                const auto& baseLeg = baseFirstChunk.leg(i);
                const auto& mergedLeg = mergedFirstChunk.leg(i);
                CHECK(util::MessageDifferencer::Equals(baseLeg, mergedLeg));
            }

            //Make sure all the other chunks are the same
            for(int i = 1; i < base->chunks_size(); ++i) {
                auto baseChunk = base->chunks(i);
                auto mergedChunk = mergedTripPtr->chunks(i);
                CHECK(util::MessageDifferencer::Equals(baseChunk, mergedChunk));
            }

            // dumpProto(std::move(base),
            //           std::move(ourChange),
            //           std::move(theirsChange),
            //           std::move(mergedTripPtr));

        }

        SECTION("Modify the same name, but changed lrud") {
            REQUIRE(trip->chunkCount() > 0);
            auto firstChunk = trip->chunk(0);
            firstChunk->setData(cwSurveyChunk::StationNameRole, 0, "b1"); //from a1 -> b1
            auto ourChange = cwSaveLoad::toProtoTrip(trip);

            firstChunk->setData(cwSurveyChunk::StationLeftRole, 0, "1.0"); //from a1 -> b1
            firstChunk->setData(cwSurveyChunk::StationUpRole, 0, "2.1"); //from a1 -> b1

            auto theirsChange = cwSaveLoad::toProtoTrip(trip);

            const google::protobuf::Message& baseRef = (*base.get());
            const google::protobuf::Message& oursRef = (*ourChange.get());
            const google::protobuf::Message& thiersRef = (*theirsChange.get());

            auto mergedTrip = cwDiff::mergeMessageByReflection(baseRef,
                                                               oursRef,
                                                               thiersRef,
                                                               cwDiff::MergeStrategy::UseOursOnConflict);
            auto mergedTripPtr = [&]()->std::unique_ptr<CavewhereProto::Trip> {
                if(auto raw = dynamic_cast<CavewhereProto::Trip*>(mergedTrip.get())) {
                    mergedTrip.release();
                    return std::unique_ptr<CavewhereProto::Trip>(raw);
                }
                return nullptr;
            }();
            REQUIRE(mergedTripPtr != nullptr);

            CHECK(!google::protobuf::util::MessageDifferencer::Equals(*base, *mergedTripPtr));
            CHECK(!google::protobuf::util::MessageDifferencer::Equals(*ourChange, *mergedTripPtr)); //Not the same because LRUD were changed
            CHECK(google::protobuf::util::MessageDifferencer::Equals(*theirsChange, *mergedTripPtr));

            int chunksFieldNumber = 5;
            auto chunkField = base->descriptor()->FindFieldByNumber(5);
            CHECK(compareMessagesIgnoring(*base, *mergedTripPtr, {chunkField}));

            REQUIRE(base->chunks_size() == 4);
            REQUIRE(mergedTripPtr->chunks_size() == 4);

            //Check that everything that the chunk are the same
            const auto& baseFirstChunk = base->chunks(0);
            const auto& mergedFirstChunk = mergedTripPtr->chunks(0);
            CHECK(!util::MessageDifferencer::Equals(baseFirstChunk, mergedFirstChunk));

            REQUIRE(baseFirstChunk.leg_size() == 9);
            REQUIRE(mergedFirstChunk.leg_size() == 9);

            const auto& baseFirstStation = baseFirstChunk.leg(0);
            const auto& mergedFirstStation = mergedFirstChunk.leg(0);

            CHECK(baseFirstStation.name() == "a1");
            CHECK(mergedFirstStation.name() == "b1");
            CHECK(mergedFirstStation.left() == "1.0");
            CHECK(mergedFirstStation.up() == "2.1");

            auto stationNameField = mergedFirstStation.descriptor()->FindFieldByNumber(100);
            auto stationLeftField = mergedFirstStation.descriptor()->FindFieldByNumber(101);
            auto stationUpField = mergedFirstStation.descriptor()->FindFieldByNumber(103);
            CHECK(compareMessagesIgnoring(baseFirstStation, mergedFirstStation, {stationNameField,
                                                                                 stationLeftField,
                                                                                 stationUpField}));

            //Make sure all the other leg data in the first chunk is the same
            for(int i = 1; i < baseFirstChunk.leg_size(); i++) {
                const auto& baseLeg = baseFirstChunk.leg(i);
                const auto& mergedLeg = mergedFirstChunk.leg(i);
                CHECK(util::MessageDifferencer::Equals(baseLeg, mergedLeg));
            }

            //Make sure all the other chunks are the same
            for(int i = 1; i < base->chunks_size(); ++i) {
                auto baseChunk = base->chunks(i);
                auto mergedChunk = mergedTripPtr->chunks(i);
                CHECK(util::MessageDifferencer::Equals(baseChunk, mergedChunk));
            }
        }

        SECTION("Field level conflict") {
            //Change just ours
            //Change just theirs
            //Change ours and theirs, take ours
            //Change ours and theirs, take theirs
            auto baseTrip = cwSaveLoad::toProtoTrip(trip);

            auto oursTrip = std::make_unique<CavewhereProto::Trip>();
            oursTrip->CopyFrom(*baseTrip);

            auto theirsTrip = std::make_unique<CavewhereProto::Trip>();
            theirsTrip->CopyFrom(*baseTrip);

            mutateScalarsRecursively(*baseTrip, *oursTrip, *theirsTrip);

            SECTION("Change just ours") {
                auto mergedTrip = cwDiff::mergeMessageByReflection(
                    *baseTrip,         // base
                    *oursTrip,         // ours
                    *baseTrip,         // theirs == base
                    cwDiff::MergeStrategy::UseOursOnConflict
                    );
                auto mergedTripPtr = [&]()->std::unique_ptr<CavewhereProto::Trip> {
                    if(auto raw = dynamic_cast<CavewhereProto::Trip*>(mergedTrip.get())) {
                        mergedTrip.release();
                        return std::unique_ptr<CavewhereProto::Trip>(raw);
                    }
                    return nullptr;
                }();
                REQUIRE(mergedTripPtr != nullptr);

                CHECK(util::MessageDifferencer::Equals(*oursTrip, *mergedTripPtr));
                CHECK(!util::MessageDifferencer::Equals(*oursTrip, *baseTrip));

                // dumpProto(std::move(baseTrip),
                //           std::move(oursTrip),
                //           std::move(theirsTrip),
                //           std::move(mergedTripPtr));
            }

            SECTION("Change just theirs") {
                auto mergedTrip = cwDiff::mergeMessageByReflection(
                    *baseTrip,         // base
                    *baseTrip,         // ours
                    *theirsTrip,         // theirs == base
                    cwDiff::MergeStrategy::UseOursOnConflict
                    );
                auto mergedTripPtr = [&]()->std::unique_ptr<CavewhereProto::Trip> {
                    if(auto raw = dynamic_cast<CavewhereProto::Trip*>(mergedTrip.get())) {
                        mergedTrip.release();
                        return std::unique_ptr<CavewhereProto::Trip>(raw);
                    }
                    return nullptr;
                }();
                REQUIRE(mergedTripPtr != nullptr);

                CHECK(util::MessageDifferencer::Equals(*theirsTrip, *mergedTripPtr));
                CHECK(!util::MessageDifferencer::Equals(*oursTrip, *baseTrip));
                CHECK(!util::MessageDifferencer::Equals(*oursTrip, *theirsTrip));

                // dumpProto(std::move(baseTrip),
                //           std::move(oursTrip),
                //           std::move(theirsTrip),
                //           std::move(mergedTripPtr));
            }

            SECTION("Change both take ours") {
                auto mergedTrip = cwDiff::mergeMessageByReflection(
                    *baseTrip,         // base
                    *oursTrip,         // ours
                    *theirsTrip,         // theirs
                    cwDiff::MergeStrategy::UseOursOnConflict
                    );
                auto mergedTripPtr = [&]()->std::unique_ptr<CavewhereProto::Trip> {
                    if(auto raw = dynamic_cast<CavewhereProto::Trip*>(mergedTrip.get())) {
                        mergedTrip.release();
                        return std::unique_ptr<CavewhereProto::Trip>(raw);
                    }
                    return nullptr;
                }();
                REQUIRE(mergedTripPtr != nullptr);

                CHECK(util::MessageDifferencer::Equals(*oursTrip, *mergedTripPtr));
                CHECK(!util::MessageDifferencer::Equals(*theirsTrip, *baseTrip));
                CHECK(!util::MessageDifferencer::Equals(*theirsTrip, *oursTrip));
            }

            SECTION("Change both take theirs") {
                auto mergedTrip = cwDiff::mergeMessageByReflection(
                    *baseTrip,         // base
                    *oursTrip,         // ours
                    *theirsTrip,         // theirs
                    cwDiff::MergeStrategy::UseTheirsOnConflict
                    );
                auto mergedTripPtr = [&]()->std::unique_ptr<CavewhereProto::Trip> {
                    if(auto raw = dynamic_cast<CavewhereProto::Trip*>(mergedTrip.get())) {
                        mergedTrip.release();
                        return std::unique_ptr<CavewhereProto::Trip>(raw);
                    }
                    return nullptr;
                }();
                REQUIRE(mergedTripPtr != nullptr);

                CHECK(util::MessageDifferencer::Equals(*theirsTrip, *mergedTripPtr));
                CHECK(!util::MessageDifferencer::Equals(*oursTrip, *baseTrip));
                CHECK(!util::MessageDifferencer::Equals(*theirsTrip, *oursTrip));

                // dumpProto(std::move(baseTrip),
                //           std::move(oursTrip),
                //           std::move(theirsTrip),
                //           std::move(mergedTripPtr));
            }
        }

        SECTION("Conflicts needs to be valid") {

            SECTION("Remove first shot") {
                //Change the first station in ours
                REQUIRE(trip->chunkCount() > 0);
                auto firstChunk = trip->chunk(0);
                firstChunk->setData(cwSurveyChunk::StationNameRole, 0, "b1"); //from a1 -> b1
                auto ourChange = cwSaveLoad::toProtoTrip(trip);

                //Remove the first station and shot in thiers
                //This removes the first station and shot, since merging is tracked on id
                //there's no conflict even through the first station in the chunk has changed.
                //This because the id, hasn't changed and will never change because it's a uuid
                //This gives valid results (for chunk) at this case,
                //but will always take the delete even if the merge
                //stratage is opposite (like this testcase).
                firstChunk->setData(cwSurveyChunk::StationNameRole, 0, "a1"); //from a1 -> b1
                firstChunk->removeStation(0, cwSurveyChunk::Below);

                auto theirsChange = cwSaveLoad::toProtoTrip(trip);

                const google::protobuf::Message& baseRef = (*base.get());
                const google::protobuf::Message& oursRef = (*ourChange.get());
                const google::protobuf::Message& thiersRef = (*theirsChange.get());

                auto mergedTrip = cwDiff::mergeMessageByReflection(baseRef,
                                                                   oursRef,
                                                                   thiersRef,
                                                                   cwDiff::MergeStrategy::UseOursOnConflict);
                auto mergedTripPtr = [&]()->std::unique_ptr<CavewhereProto::Trip> {
                    if(auto raw = dynamic_cast<CavewhereProto::Trip*>(mergedTrip.get())) {
                        mergedTrip.release();
                        return std::unique_ptr<CavewhereProto::Trip>(raw);
                    }
                    return nullptr;
                }();
                REQUIRE(mergedTripPtr != nullptr);

                CHECK(!google::protobuf::util::MessageDifferencer::Equals(*base, *mergedTripPtr));
                CHECK(!google::protobuf::util::MessageDifferencer::Equals(*ourChange, *mergedTripPtr)); //Not the same because LRUD were changed
                CHECK(google::protobuf::util::MessageDifferencer::Equals(*theirsChange, *mergedTripPtr));

                int chunksFieldNumber = 5;
                auto chunkField = base->descriptor()->FindFieldByNumber(5);
                CHECK(compareMessagesIgnoring(*base, *mergedTripPtr, {chunkField}));

                REQUIRE(base->chunks_size() == 4);
                REQUIRE(mergedTripPtr->chunks_size() == 4);

                //Check that everything that the chunk are the same
                const auto& baseFirstChunk = base->chunks(0);
                const auto& mergedFirstChunk = mergedTripPtr->chunks(0);
                CHECK(!util::MessageDifferencer::Equals(baseFirstChunk, mergedFirstChunk));

                REQUIRE(baseFirstChunk.leg_size() == 9);
                REQUIRE(mergedFirstChunk.leg_size() == 7); //We delete the first and last station

                const auto& baseFirstStation = baseFirstChunk.leg(0);
                const auto& mergedFirstStation = mergedFirstChunk.leg(0);

                //Make sure all the other leg data in the first chunk is the same
                for(int i = 2; i < baseFirstChunk.leg_size(); i++) {
                    const auto& baseLeg = baseFirstChunk.leg(i);
                    const auto& mergedLeg = mergedFirstChunk.leg(i - 2);
                    CHECK(util::MessageDifferencer::Equals(baseLeg, mergedLeg));
                }

                //Make sure all the other chunks are the same
                for(int i = 1; i < base->chunks_size(); ++i) {
                    auto baseChunk = base->chunks(i);
                    auto mergedChunk = mergedTripPtr->chunks(i);
                    CHECK(util::MessageDifferencer::Equals(baseChunk, mergedChunk));
                }

                // dumpProto(std::move(base),
                //           std::move(ourChange),
                //           std::move(theirsChange),
                //           std::move(mergedTripPtr));
            }
        }
    }
}
