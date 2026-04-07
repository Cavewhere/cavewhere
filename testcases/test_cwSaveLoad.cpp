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
#include "ProjectFilenameTestHelper.h"
#include "cwError.h"
#include "cwErrorListModel.h"
#include "cwFutureManagerModel.h"
#include "cwCavingRegion.h"
#include "cwScrap.h"
#include "cwNoteStation.h"
#include "cwLead.h"
#include "cwNameUtils.h"
#include "cwNoteLiDARData.h"
#include "cwNoteLiDARStation.h"
#include "LoadProjectHelper.h"
#include <asyncfuture.h>

//QQuickGit includes
#include "GitRepository.h"

//Qt includes
#include <QStandardPaths>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#ifdef CW_WITH_PDF_SUPPORT
#include <QPdfDocument>
#endif
#include <QDate>
#include <QHash>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>

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

QString firstCwprojInDirectory(const QString& rootPath)
{
    QDirIterator it(rootPath,
                    QStringList() << QStringLiteral("*.cwproj"),
                    QDir::Files,
                    QDirIterator::Subdirectories);

    while (it.hasNext()) {
        const QString candidate = it.next();
        if (candidate.contains(QStringLiteral("__MACOSX"))) {
            continue;
        }

        const QFileInfo info(candidate);
        if (info.fileName().startsWith(QStringLiteral("._"))) {
            continue;
        }

        return candidate;
    }

    return {};
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

    const QDir notesDir = ProjectFilenameTestHelper::dir(trip->notes());
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
    const QString projectPath = QDir(tempDir.path()).filePath(QStringLiteral("version-test.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();

    auto checkFileVersion = [](const auto& proto) {
        REQUIRE(proto.has_fileversion());
        CHECK(proto.fileversion().version() == cwRegionIOTask::protoVersion());
        CHECK(proto.fileversion().cavewhereversion() == CavewhereVersion.toStdString());
    };

    auto checkHasId = [](const auto& proto) {
        REQUIRE(proto.has_id());
        CHECK(!proto.id().empty());
    };

    const QString caveFile = ProjectFilenameTestHelper::absolutePath(cave);
    const QString tripFile = ProjectFilenameTestHelper::absolutePath(trip);
    const QString noteFile = ProjectFilenameTestHelper::absolutePath(note);
    const QString noteLidarFile = ProjectFilenameTestHelper::absolutePath(noteLidar);

    const auto caveProto = loadProtoFromJsonFile<CavewhereProto::Cave>(caveFile);
    const auto tripProto = loadProtoFromJsonFile<CavewhereProto::Trip>(tripFile);
    const auto noteProto = loadProtoFromJsonFile<CavewhereProto::Note>(noteFile);
    const auto noteLidarProto = loadProtoFromJsonFile<CavewhereProto::NoteLiDAR>(noteLidarFile);

    checkFileVersion(caveProto);
    checkFileVersion(tripProto);
    checkFileVersion(noteProto);
    checkFileVersion(noteLidarProto);

    checkHasId(caveProto);
    checkHasId(tripProto);
    checkHasId(noteProto);
    checkHasId(noteLidarProto);
}

TEST_CASE("cwSaveLoad setFileName does not initialize git repository", "[cwSaveLoad]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString projectPath = QDir(tempDir.path()).filePath(QStringLiteral("setter-only.cwproj"));
    cwSaveLoad saveLoad;
    saveLoad.setFileName(projectPath);

    CHECK_FALSE(QFileInfo::exists(QDir(tempDir.path()).filePath(QStringLiteral(".git"))));
}

TEST_CASE("cwSaveLoad loading non-git project does not initialize repository", "[cwSaveLoad]") {
    auto creatorRoot = std::make_unique<cwRootData>();
    auto creatorProject = creatorRoot->project();
    auto creatorRegion = creatorProject->cavingRegion();

    creatorRegion->addCave();
    auto cave = creatorRegion->cave(0);
    REQUIRE(cave != nullptr);
    cave->addTrip();

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString projectPath = QDir(tempDir.path()).filePath(QStringLiteral("non-git-load.cwproj"));
    REQUIRE(creatorProject->saveAs(projectPath));
    creatorProject->waitSaveToFinish();

    const QDir projectDir = QFileInfo(projectPath).absoluteDir();
    const QString gitPath = projectDir.filePath(QStringLiteral(".git"));
    if (QFileInfo::exists(gitPath)) {
        REQUIRE(QDir(gitPath).removeRecursively());
    }
    REQUIRE_FALSE(QFileInfo::exists(gitPath));

    auto loaderProject = std::make_unique<cwProject>();
    addTokenManager(loaderProject.get());
    loaderProject->loadOrConvert(projectPath);
    loaderProject->waitLoadToFinish();

    CHECK_FALSE(QFileInfo::exists(gitPath));
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
        clearImageFields(ProjectFilenameTestHelper::absolutePath(note));
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
        const QString absolutePath = ProjectFilenameTestHelper::absolutePath(note, note->image().path());
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

    CHECK(root->project()->fileType() == cwProject::GitFileType);

    const QDir workingDataRoot = root->project()->dataRootDir();
    const QDir workingProjectRoot = QFileInfo(workingDataRoot.absolutePath()).absoluteDir();
    QString convertedFilename = firstCwprojInDirectory(workingProjectRoot.absolutePath());
    REQUIRE_FALSE(convertedFilename.isEmpty());
    CHECK(QFileInfo::exists(convertedFilename));
    CHECK(convertedFilename.endsWith(QStringLiteral(".cwproj")));

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

// Compression rule 1: multiple WriteFile jobs for the same object — only the last is kept.
// Verified by changing trip date three times before the flush runs, then checking that
// the saved file reflects the final date.
TEST_CASE("cwSaveLoad compression rule 1: last write wins when multiple writes queue for the same object", "[cwSaveLoad]") {
    auto root = std::make_unique<cwRootData>();
    auto project = root->project();
    auto region = project->cavingRegion();

    region->addCave();
    auto cave = region->cave(0);
    cave->addTrip();
    auto trip = cave->trip(0);

    root->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    // Change date three times synchronously — each change queues a WriteFile for the
    // same trip before the flush gets to run. Only the last write should survive.
    const QDate date1(2020, 1, 1);
    const QDate date2(2021, 6, 15);
    const QDate date3(2023, 11, 30);
    trip->setDate(QDateTime(date1, QTime(), QTimeZone::utc()));
    trip->setDate(QDateTime(date2, QTime(), QTimeZone::utc()));
    trip->setDate(QDateTime(date3, QTime(), QTimeZone::utc()));

    project->waitSaveToFinish();

    const auto tripProto = loadProtoFromJsonFile<CavewhereProto::Trip>(
        ProjectFilenameTestHelper::absolutePath(trip));

    REQUIRE(tripProto.has_date());
    CHECK(tripProto.date().year()  == date3.year());
    CHECK(tripProto.date().month() == date3.month());
    CHECK(tripProto.date().day()   == date3.day());
}

// Compression rule 2: a WriteFile queued before a Remove for the same object is dropped.
// Verified by adding a trip and immediately removing it, then checking that no directory
// for that trip was ever created on disk.
TEST_CASE("cwSaveLoad compression rule 2: adding then immediately removing a trip leaves no files on disk", "[cwSaveLoad]") {
    auto root = std::make_unique<cwRootData>();
    auto project = root->project();
    auto region = project->cavingRegion();

    region->addCave();
    auto cave = region->cave(0);

    root->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    // Add a trip — queues WriteFile. Capture its expected directory before removal.
    cave->addTrip();
    auto trip = cave->trip(0);
    trip->setName("Doomed Trip");
    const QString expectedTripDir = ProjectFilenameTestHelper::dir(trip).absolutePath();

    // Remove immediately — queues Directory::Remove for the same trip before flush runs.
    cave->removeTrip(0);

    project->waitSaveToFinish();

    CHECK(!QFileInfo::exists(expectedTripDir));
}

// Compression rule 3: sequential Move jobs for the same object collapse to a single move.
// Verified by renaming a cave twice before the flush runs, then checking that only the
// final directory exists and the intermediate name was never created on disk.
TEST_CASE("cwSaveLoad compression rule 3: renaming an object twice before flush moves directly to the final name", "[cwSaveLoad]") {
    auto root = std::make_unique<cwRootData>();
    auto project = root->project();
    auto region = project->cavingRegion();

    region->addCave();
    auto cave = region->cave(0);
    cave->setName("Alpha");

    root->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    const QDir alphaDir = ProjectFilenameTestHelper::dir(cave);
    REQUIRE(alphaDir.exists());

    // Rename twice synchronously — stacks two Move chains (dir + file) before the flush.
    cave->setName("Beta");
    const QDir betaDir = ProjectFilenameTestHelper::dir(cave);

    cave->setName("Gamma");
    const QDir gammaDir = ProjectFilenameTestHelper::dir(cave);

    project->waitSaveToFinish();

    CHECK(gammaDir.exists());
    CHECK(!betaDir.exists());
    CHECK(!alphaDir.exists());
}

TEST_CASE("enqueueFlushAndCommit creates a git commit after saveAs from a temporary project", "[cwSaveLoad]") {
    auto root = std::make_unique<cwRootData>();
    auto project = root->project();
    auto region = project->cavingRegion();

    root->account()->setName(QStringLiteral("Test User"));
    root->account()->setEmail(QStringLiteral("test@example.com"));

    region->addCave();
    region->cave(0)->setName("Test Cave");

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectPath = QDir(tempDir.path()).filePath(QStringLiteral("commit-test/commit-test.cwproj"));

    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();

    auto* repo = project->repository();
    REQUIRE(repo != nullptr);

    // A HEAD commit must exist. Without it every project file is untracked and
    // a subsequent discardChanges() (reset --hard HEAD + cleanUntracked) would
    // delete the entire project from disk.
    const auto headOid = QQuickGit::GitRepository::headCommitOid(repo->directory().absolutePath());
    CHECK_FALSE(headOid.hasError());
    CHECK(!headOid.value().isEmpty());

    // No untracked or modified files — nothing for cleanUntracked() to delete.
    repo->checkStatus();
    CHECK(repo->modifiedFileCount() == 0);
}

TEST_CASE("enqueueFlushAndCommit creates a new commit on each save() call", "[cwSaveLoad]") {
    auto root = std::make_unique<cwRootData>();
    auto project = root->project();
    auto region = project->cavingRegion();

    root->account()->setName(QStringLiteral("Test User"));
    root->account()->setEmail(QStringLiteral("test@example.com"));

    region->addCave();

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectPath = QDir(tempDir.path()).filePath(QStringLiteral("save-commit-test/save-commit-test.cwproj"));

    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();

    auto* repo = project->repository();
    REQUIRE(repo != nullptr);

    const auto firstOid = QQuickGit::GitRepository::headCommitOid(repo->directory().absolutePath());
    REQUIRE_FALSE(firstOid.hasError());
    REQUIRE(!firstOid.value().isEmpty());

    // Modify and save again — a second distinct commit must be produced.
    region->cave(0)->setName("Renamed Cave");
    project->waitSaveToFinish();
    REQUIRE(project->save());
    project->waitSaveToFinish();

    const auto secondOid = QQuickGit::GitRepository::headCommitOid(repo->directory().absolutePath());
    REQUIRE_FALSE(secondOid.hasError());
    CHECK(secondOid.value() != firstOid.value());

    repo->checkStatus();
    CHECK(repo->modifiedFileCount() == 0);
}

namespace {
bool bumpFileVersion(const QString& filePath, int newVersion)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) {
        return false;
    }

    QJsonObject root = doc.object();
    QJsonObject fileVersion = root[QStringLiteral("fileVersion")].toObject();
    if (fileVersion.isEmpty()) {
        return false;
    }
    fileVersion[QStringLiteral("version")] = newVersion;
    root[QStringLiteral("fileVersion")] = fileVersion;

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson());
    return true;
}

QStringList findEntityFiles(const QString& rootPath, const QStringList& suffixes)
{
    QStringList result;
    QDirIterator it(rootPath, suffixes, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        result.append(it.next());
    }
    result.sort();
    return result;
}
}

TEST_CASE("loadAll collects version warnings for newer entity files", "[cwSaveLoad]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    cave->setName(QStringLiteral("VersionTestCave"));
    cave->addTrip();
    auto trip = cave->trip(0);
    trip->setName(QStringLiteral("VersionTestTrip"));

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectPath = QDir(tempDir.path()).filePath(QStringLiteral("VersionTest.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    const QString savedProjectFile = project->filename();
    REQUIRE(QFileInfo::exists(savedProjectFile));

    const int futureVersion = cwRegionIOTask::protoVersion() + 1;

    SECTION("No warnings when all entities at current version") {
        auto loadFuture = cwSaveLoad::loadAll(savedProjectFile);
        REQUIRE(AsyncFuture::waitForFinished(loadFuture, 10000));
        REQUIRE_FALSE(loadFuture.result().hasError());
        CHECK(loadFuture.result().value().errors.isEmpty());
    }

    SECTION("Warning when cave file has future version") {
        auto caveFiles = findEntityFiles(QFileInfo(savedProjectFile).absolutePath(), {QStringLiteral("*.cwcave")});
        REQUIRE(caveFiles.size() == 1);
        REQUIRE(bumpFileVersion(caveFiles.first(), futureVersion));

        auto loadFuture = cwSaveLoad::loadAll(savedProjectFile);
        REQUIRE(AsyncFuture::waitForFinished(loadFuture, 10000));
        REQUIRE_FALSE(loadFuture.result().hasError());

        const auto& errors = loadFuture.result().value().errors;
        REQUIRE(errors.size() == 1);
        CHECK(errors.first().type() == cwError::Warning);
        CHECK(errors.first().message().contains(QStringLiteral("newer version")));

        // Data should still load (best-effort)
        CHECK(loadFuture.result().value().region.caves.size() == 1);
    }

    SECTION("Warning when trip file has future version") {
        auto tripFiles = findEntityFiles(QFileInfo(savedProjectFile).absolutePath(), {QStringLiteral("*.cwtrip")});
        REQUIRE(tripFiles.size() == 1);
        REQUIRE(bumpFileVersion(tripFiles.first(), futureVersion));

        auto loadFuture = cwSaveLoad::loadAll(savedProjectFile);
        REQUIRE(AsyncFuture::waitForFinished(loadFuture, 10000));
        REQUIRE_FALSE(loadFuture.result().hasError());

        const auto& errors = loadFuture.result().value().errors;
        REQUIRE(errors.size() == 1);
        CHECK(errors.first().type() == cwError::Warning);

        // Data should still load (best-effort)
        CHECK(loadFuture.result().value().region.caves.size() == 1);
        CHECK(loadFuture.result().value().region.caves.first().trips.size() == 1);
    }

    SECTION("Warning when project file has future version") {
        auto projFiles = findEntityFiles(QFileInfo(savedProjectFile).absolutePath(), {QStringLiteral("*.cwproj")});
        REQUIRE(projFiles.size() == 1);
        REQUIRE(bumpFileVersion(projFiles.first(), futureVersion));

        auto loadFuture = cwSaveLoad::loadAll(savedProjectFile);
        REQUIRE(AsyncFuture::waitForFinished(loadFuture, 10000));
        REQUIRE_FALSE(loadFuture.result().hasError());

        const auto& errors = loadFuture.result().value().errors;
        REQUIRE(errors.size() == 1);
        CHECK(errors.first().type() == cwError::Warning);
        CHECK(errors.first().message().contains(QStringLiteral("newer version")));

        // Data should still load (best-effort)
        CHECK(loadFuture.result().value().region.caves.size() == 1);
    }

    SECTION("Multiple warnings when both cave and trip have future versions") {
        auto caveFiles = findEntityFiles(QFileInfo(savedProjectFile).absolutePath(), {QStringLiteral("*.cwcave")});
        auto tripFiles = findEntityFiles(QFileInfo(savedProjectFile).absolutePath(), {QStringLiteral("*.cwtrip")});
        REQUIRE(caveFiles.size() == 1);
        REQUIRE(tripFiles.size() == 1);
        REQUIRE(bumpFileVersion(caveFiles.first(), futureVersion));
        REQUIRE(bumpFileVersion(tripFiles.first(), futureVersion));

        auto loadFuture = cwSaveLoad::loadAll(savedProjectFile);
        REQUIRE(AsyncFuture::waitForFinished(loadFuture, 10000));
        REQUIRE_FALSE(loadFuture.result().hasError());

        const auto& errors = loadFuture.result().value().errors;
        CHECK(errors.size() == 2);
        for (const auto& error : errors) {
            CHECK(error.type() == cwError::Warning);
        }
    }
}

TEST_CASE("cwProject surfaces load errors to errorModel for git projects", "[cwSaveLoad]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    cave->setName(QStringLiteral("ErrorModelCave"));
    cave->addTrip();
    cave->trip(0)->setName(QStringLiteral("ErrorModelTrip"));

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectPath = QDir(tempDir.path()).filePath(QStringLiteral("ErrorModelTest.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    const QString savedProjectFile = project->filename();
    REQUIRE(QFileInfo::exists(savedProjectFile));

    // Bump cave version to future
    auto caveFiles = findEntityFiles(QFileInfo(savedProjectFile).absolutePath(), {QStringLiteral("*.cwcave")});
    REQUIRE(caveFiles.size() == 1);
    const int futureVersion = cwRegionIOTask::protoVersion() + 1;
    REQUIRE(bumpFileVersion(caveFiles.first(), futureVersion));

    // Reload the project through cwProject (git path)
    auto reloaded = std::make_unique<cwProject>();
    addTokenManager(reloaded.get());
    reloaded->loadOrConvert(savedProjectFile);
    reloaded->waitLoadToFinish();

    // errorModel should contain the version warning
    REQUIRE(reloaded->errorModel()->count() >= 1);
    bool foundVersionWarning = false;
    for (int i = 0; i < reloaded->errorModel()->count(); ++i) {
        const auto error = reloaded->errorModel()->at(i);
        if (error.type() == cwError::Warning && error.message().contains(QStringLiteral("newer version"))) {
            foundVersionWarning = true;
            break;
        }
    }
    CHECK(foundVersionWarning);

    // Data should still be loaded (best-effort)
    CHECK(reloaded->cavingRegion()->caveCount() == 1);
}

TEST_CASE("Entity saves blocked with errors when project has newer version entities", "[cwSaveLoad]") {
    // Set up a project with cave, trip, note, and LiDAR note
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    cave->setName(QStringLiteral("SaveBlockCave"));
    cave->addTrip();
    auto trip = cave->trip(0);
    trip->setName(QStringLiteral("SaveBlockTrip"));

    // Add a note image and a LiDAR note
    const QString noteImagePath = copyToTempFolder("://datasets/test_cwNote/testpage.png");
    trip->notes()->addFromFiles({QUrl::fromLocalFile(noteImagePath)});

    const QString glbPath = copyToTempFolder("://datasets/test_cwSurveyNotesConcatModel/bones.glb");
    trip->notesLiDAR()->addFromFiles({QUrl::fromLocalFile(glbPath)});

    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    REQUIRE(trip->notes()->notes().size() == 1);
    REQUIRE(trip->notesLiDAR()->notes().size() == 1);

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectPath = QDir(tempDir.path()).filePath(QStringLiteral("SaveBlockTest.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    const QString savedProjectFile = project->filename();
    REQUIRE(QFileInfo::exists(savedProjectFile));

    // Bump cave version to future
    auto caveFiles = findEntityFiles(QFileInfo(savedProjectFile).absolutePath(), {QStringLiteral("*.cwcave")});
    REQUIRE(caveFiles.size() == 1);
    const int futureVersion = cwRegionIOTask::protoVersion() + 1;
    REQUIRE(bumpFileVersion(caveFiles.first(), futureVersion));

    // Reload the project through cwProject (git path)
    auto reloaded = std::make_unique<cwProject>();
    addTokenManager(reloaded.get());
    reloaded->loadOrConvert(savedProjectFile);
    reloaded->waitLoadToFinish();

    REQUIRE(reloaded->cavingRegion()->caveCount() == 1);
    auto* reloadedCave = reloaded->cavingRegion()->cave(0);
    auto* reloadedTrip = reloadedCave->trip(0);
    REQUIRE(reloadedTrip->notes()->notes().size() == 1);
    REQUIRE(reloadedTrip->notesLiDAR()->notes().size() == 1);
    auto* reloadedNote = reloadedTrip->notes()->notes().value(0);
    auto* reloadedLiDAR = qobject_cast<cwNoteLiDAR*>(reloadedTrip->notesLiDAR()->notes().value(0));
    REQUIRE(reloadedNote != nullptr);
    REQUIRE(reloadedLiDAR != nullptr);

    // Clear load errors so we can check for save-specific errors
    reloaded->errorModel()->clear();

    auto checkSaveBlocked = [&](const QString& entityType) {
        INFO("Checking save blocked for " << entityType.toStdString());
        REQUIRE(reloaded->errorModel()->count() >= 1);
        const auto& lastError = reloaded->errorModel()->at(reloaded->errorModel()->count() - 1);
        CHECK(lastError.type() == cwError::Warning);
        CHECK(lastError.message().contains(QStringLiteral("Cannot save")));
        CHECK(lastError.message().contains(QStringLiteral("newer version")));
    };

    SECTION("save(cwCave*) is blocked when cave name changes") {
        reloadedCave->setName(QStringLiteral("ModifiedCaveName"));
        reloaded->waitSaveToFinish();
        checkSaveBlocked(QStringLiteral("cwCave"));
    }

    SECTION("save(cwTrip*) is blocked when trip date changes") {
        reloadedTrip->setDate(QDateTime(QDate(2025, 6, 15), QTime()));
        reloaded->waitSaveToFinish();
        checkSaveBlocked(QStringLiteral("cwTrip"));
    }

    SECTION("save(cwNote*) is blocked when note rotation changes") {
        reloadedNote->setRotate(45.0);
        reloaded->waitSaveToFinish();
        checkSaveBlocked(QStringLiteral("cwNote"));
    }

    SECTION("save(cwNoteLiDAR*) is blocked when LiDAR name changes") {
        reloadedLiDAR->setName(QStringLiteral("ModifiedLiDARName"));
        reloaded->waitSaveToFinish();
        checkSaveBlocked(QStringLiteral("cwNoteLiDAR"));
    }

    // Note: saveCavingRegion() is guarded but not signal-connected in the current code.
    // Region name changes trigger directory renames, not saveCavingRegion().
    // The guard exists as defense-in-depth for future callers.
}

TEST_CASE("addImages and addFiles blocked when project has newer version entities", "[cwSaveLoad]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    cave->setName(QStringLiteral("BlockAddFilesCave"));
    cave->addTrip();
    cave->trip(0)->setName(QStringLiteral("BlockAddFilesTrip"));

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectPath = QDir(tempDir.path()).filePath(QStringLiteral("BlockAddFilesTest.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    const QString savedProjectFile = project->filename();
    REQUIRE(QFileInfo::exists(savedProjectFile));

    // Bump cave version to future
    auto caveFiles = findEntityFiles(QFileInfo(savedProjectFile).absolutePath(), {QStringLiteral("*.cwcave")});
    REQUIRE(caveFiles.size() == 1);
    const int futureVersion = cwRegionIOTask::protoVersion() + 1;
    REQUIRE(bumpFileVersion(caveFiles.first(), futureVersion));

    // Reload the project through cwProject (git path)
    auto reloaded = std::make_unique<cwProject>();
    addTokenManager(reloaded.get());
    reloaded->loadOrConvert(savedProjectFile);
    reloaded->waitLoadToFinish();

    // Clear load errors so we can check for add-specific errors
    reloaded->errorModel()->clear();

    // Create a dummy image file to attempt to add
    const QString dummyImagePath = QDir(tempDir.path()).filePath(QStringLiteral("dummy.png"));
    {
        QFile dummyFile(dummyImagePath);
        REQUIRE(dummyFile.open(QIODevice::WriteOnly));
        dummyFile.write("not a real image");
    }

    SECTION("addImages is blocked") {
        bool callbackCalled = false;
        reloaded->addImages(
            { QUrl::fromLocalFile(dummyImagePath) },
            QDir(tempDir.path()),
            [&callbackCalled](QList<cwImage>) { callbackCalled = true; });
        rootData->futureManagerModel()->waitForFinished();

        CHECK_FALSE(callbackCalled);
        REQUIRE(reloaded->errorModel()->count() >= 1);
        CHECK(reloaded->errorModel()->at(0).type() == cwError::Warning);
        CHECK(reloaded->errorModel()->at(0).message().contains(QStringLiteral("Cannot add images")));
    }

    SECTION("addFiles is blocked") {
        bool callbackCalled = false;
        reloaded->addFiles(
            { QUrl::fromLocalFile(dummyImagePath) },
            QDir(tempDir.path()),
            [&callbackCalled](QList<QString>) { callbackCalled = true; });
        rootData->futureManagerModel()->waitForFinished();

        CHECK_FALSE(callbackCalled);
        REQUIRE(reloaded->errorModel()->count() >= 1);
        CHECK(reloaded->errorModel()->at(0).type() == cwError::Warning);
        CHECK(reloaded->errorModel()->at(0).message().contains(QStringLiteral("Cannot add files")));
    }
}

TEST_CASE("Version-incompatible project is not marked as temporary", "[cwSaveLoad]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    auto region = project->cavingRegion();
    region->addCave();
    region->cave(0)->setName(QStringLiteral("NotTempCave"));

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectPath = QDir(tempDir.path()).filePath(QStringLiteral("NotTempTest.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    const QString savedProjectFile = project->filename();

    // Bump cave version to future
    auto caveFiles = findEntityFiles(QFileInfo(savedProjectFile).absolutePath(), {QStringLiteral("*.cwcave")});
    REQUIRE(caveFiles.size() == 1);
    REQUIRE(bumpFileVersion(caveFiles.first(), cwRegionIOTask::protoVersion() + 1));

    auto reloaded = std::make_unique<cwProject>();
    addTokenManager(reloaded.get());
    reloaded->loadOrConvert(savedProjectFile);
    reloaded->waitLoadToFinish();

    CHECK(reloaded->saveWillCauseDataLoss());
    CHECK_FALSE(reloaded->isTemporaryProject());
    CHECK_FALSE(reloaded->canSaveDirectly());
}

TEST_CASE("deleteTemporaryProject rejected for version-incompatible project", "[cwSaveLoad]") {
    // A version-incompatible project loaded from a real path is not temporary,
    // so deleteTemporaryProject should reject it as "not temporary".
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    auto region = project->cavingRegion();
    region->addCave();
    region->cave(0)->setName(QStringLiteral("DeleteRejectCave"));

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectPath = QDir(tempDir.path()).filePath(QStringLiteral("DeleteRejectTest.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    const QString savedProjectFile = project->filename();

    auto caveFiles = findEntityFiles(QFileInfo(savedProjectFile).absolutePath(), {QStringLiteral("*.cwcave")});
    REQUIRE(caveFiles.size() == 1);
    REQUIRE(bumpFileVersion(caveFiles.first(), cwRegionIOTask::protoVersion() + 1));

    auto reloaded = std::make_unique<cwProject>();
    addTokenManager(reloaded.get());
    reloaded->loadOrConvert(savedProjectFile);
    reloaded->waitLoadToFinish();

    reloaded->errorModel()->clear();

    CHECK(reloaded->saveWillCauseDataLoss());
    CHECK_FALSE(reloaded->isTemporaryProject());
    CHECK_FALSE(reloaded->deleteTemporaryProject());
    REQUIRE(reloaded->errorModel()->count() >= 1);
    CHECK(reloaded->errorModel()->at(0).message().contains(QStringLiteral("not temporary")));
}

TEST_CASE("saveBlockedByVersion signal only emitted once per load", "[cwSaveLoad]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    cave->setName(QStringLiteral("SignalDedupCave"));
    cave->addTrip();
    cave->trip(0)->setName(QStringLiteral("SignalDedupTrip"));

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectPath = QDir(tempDir.path()).filePath(QStringLiteral("SignalDedupTest.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    const QString savedProjectFile = project->filename();

    auto caveFiles = findEntityFiles(QFileInfo(savedProjectFile).absolutePath(), {QStringLiteral("*.cwcave")});
    REQUIRE(caveFiles.size() == 1);
    REQUIRE(bumpFileVersion(caveFiles.first(), cwRegionIOTask::protoVersion() + 1));

    auto reloaded = std::make_unique<cwProject>();
    addTokenManager(reloaded.get());
    reloaded->loadOrConvert(savedProjectFile);
    reloaded->waitLoadToFinish();

    reloaded->errorModel()->clear();

    auto* reloadedCave = reloaded->cavingRegion()->cave(0);

    // Multiple property changes should only produce one error
    reloadedCave->setName(QStringLiteral("Change1"));
    reloaded->waitSaveToFinish();
    reloadedCave->setName(QStringLiteral("Change2"));
    reloaded->waitSaveToFinish();
    reloadedCave->setName(QStringLiteral("Change3"));
    reloaded->waitSaveToFinish();

    CHECK(reloaded->errorModel()->count() == 1);
}

TEST_CASE("saveFlushCompleted fires after adding a trip", "[cwSaveLoad]") {
    auto root = std::make_unique<cwRootData>();
    auto project = root->project();
    auto region = project->cavingRegion();

    region->addCave();
    auto cave = region->cave(0);

    root->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    // Listen for saveFlushCompleted on the project
    QSignalSpy flushSpy(project, &cwProject::saveFlushCompleted);

    // Add a trip — this queues a file write and schedules a flush
    cave->addTrip();

    project->waitSaveToFinish();

    CHECK(flushSpy.count() >= 1);

    // After the flush, the new trip file should exist on disk
    auto trip = cave->trip(0);
    const QString tripPath = ProjectFilenameTestHelper::absolutePath(trip);
    CHECK(QFileInfo::exists(tripPath));
}

TEST_CASE("saveFlushCompleted fires after modifying a trip", "[cwSaveLoad]") {
    auto root = std::make_unique<cwRootData>();
    auto project = root->project();
    auto region = project->cavingRegion();

    region->addCave();
    auto cave = region->cave(0);
    cave->addTrip();

    root->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    QSignalSpy flushSpy(project, &cwProject::saveFlushCompleted);

    // Modify the trip — triggers a save and flush
    auto trip = cave->trip(0);
    trip->setDate(QDateTime(QDate(2025, 1, 1), QTime(), QTimeZone::utc()));

    project->waitSaveToFinish();

    CHECK(flushSpy.count() >= 1);
}

TEST_CASE("git working tree detects new file after saveFlushCompleted", "[cwSaveLoad]") {
    auto root = std::make_unique<cwRootData>();
    auto project = root->project();
    auto region = project->cavingRegion();
    auto* repo = project->repository();

    region->addCave();
    auto cave = region->cave(0);

    root->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    // Ensure clean baseline
    repo->checkStatus();
    int baselineCount = repo->modifiedFileCount();

    // Add a trip — queues file write
    cave->addTrip();

    // Wait for flush to complete
    project->waitSaveToFinish();

    // Now check git status — the new file should be visible
    repo->checkStatus();
    CHECK(repo->modifiedFileCount() > baselineCount);
}

TEST_CASE("saveAs, sync, and resetBranchAndReconcile blocked for version-incompatible project", "[cwSaveLoad]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    auto region = project->cavingRegion();
    region->addCave();
    region->cave(0)->setName(QStringLiteral("GuardTestCave"));

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectPath = QDir(tempDir.path()).filePath(QStringLiteral("GuardTest.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    const QString savedProjectFile = project->filename();

    auto caveFiles = findEntityFiles(QFileInfo(savedProjectFile).absolutePath(), {QStringLiteral("*.cwcave")});
    REQUIRE(caveFiles.size() == 1);
    REQUIRE(bumpFileVersion(caveFiles.first(), cwRegionIOTask::protoVersion() + 1));

    auto reloaded = std::make_unique<cwProject>();
    addTokenManager(reloaded.get());
    reloaded->loadOrConvert(savedProjectFile);
    reloaded->waitLoadToFinish();

    REQUIRE(reloaded->saveWillCauseDataLoss());
    reloaded->errorModel()->clear();

    SECTION("saveAs is blocked") {
        const QString saveAsPath = QDir(tempDir.path()).filePath(QStringLiteral("GuardTestCopy.cwproj"));
        CHECK_FALSE(reloaded->saveAs(saveAsPath));
        REQUIRE(reloaded->errorModel()->count() >= 1);
        CHECK(reloaded->errorModel()->at(0).message().contains(QStringLiteral("Cannot save")));
    }

    SECTION("sync is blocked") {
        CHECK_FALSE(reloaded->sync());
        REQUIRE(reloaded->errorModel()->count() >= 1);
        CHECK(reloaded->errorModel()->at(0).message().contains(QStringLiteral("Cannot sync")));
    }

    SECTION("resetBranchAndReconcile is blocked") {
        CHECK_FALSE(reloaded->resetBranchAndReconcile(QStringLiteral("origin/main")));
        REQUIRE(reloaded->errorModel()->count() >= 1);
        CHECK(reloaded->errorModel()->at(0).message().contains(QStringLiteral("Cannot reconcile")));
    }

    SECTION("restoreToCommit is blocked") {
        CHECK_FALSE(reloaded->restoreToCommit(QStringLiteral("abcdef1234567890abcdef1234567890abcdef12")));
        REQUIRE(reloaded->errorModel()->count() >= 1);
        CHECK(reloaded->errorModel()->at(0).message().contains(QStringLiteral("Cannot restore")));
    }
}

TEST_CASE("cwProject restoreToCommit restores project data to a previous save", "[cwSaveLoad][RestoreToCommit]")
{
    auto root = std::make_unique<cwRootData>();
    auto project = root->project();
    auto region = project->cavingRegion();

    root->account()->setName(QStringLiteral("Test User"));
    root->account()->setEmail(QStringLiteral("test@example.com"));

    // Save 1: one cave, no trips
    region->addCave();
    region->cave(0)->setName(QStringLiteral("Restore Cave"));

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectPath = QDir(tempDir.path()).filePath(QStringLiteral("restore-test/restore-test.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();

    auto* repo = project->repository();
    REQUIRE(repo != nullptr);
    const QString repoPath = repo->directory().absolutePath();
    const auto firstOid = QQuickGit::GitRepository::headCommitOid(repoPath);
    REQUIRE_FALSE(firstOid.hasError());

    // Save 2: add a trip
    region->cave(0)->addTrip();
    region->cave(0)->trip(0)->setName(QStringLiteral("Survey Trip 1"));
    REQUIRE(project->save());
    project->waitSaveToFinish();

    const auto secondOid = QQuickGit::GitRepository::headCommitOid(repoPath);
    REQUIRE_FALSE(secondOid.hasError());
    REQUIRE(secondOid.value() != firstOid.value());

    SECTION("Restore updates in-memory model to match first save") {
        CHECK(project->restoreToCommit(firstOid.value()));
        project->waitForSyncToFinish();

        // After restore, the cave should exist but with no trips
        REQUIRE(region->caveCount() == 1);
        CHECK(region->cave(0)->name() == QStringLiteral("Restore Cave"));
        CHECK(region->cave(0)->tripCount() == 0);
    }

    SECTION("Git history shows 3 commits after restore") {
        CHECK(project->restoreToCommit(firstOid.value()));
        project->waitForSyncToFinish();

        const auto restoreOid = QQuickGit::GitRepository::headCommitOid(repoPath);
        REQUIRE_FALSE(restoreOid.hasError());
        CHECK(restoreOid.value() != firstOid.value());
        CHECK(restoreOid.value() != secondOid.value());

        // Restore commit's parent should be the second save
        auto parentResult = QQuickGit::GitRepository::commitParentOids(repoPath, restoreOid.value());
        REQUIRE_FALSE(parentResult.hasError());
        REQUIRE(parentResult.value().size() == 1);
        CHECK(parentResult.value().first() == secondOid.value());
    }

    SECTION("Invalid SHA propagates error to errorModel") {
        CHECK(project->restoreToCommit(QStringLiteral("badc0ffeebadc0ffeebadc0ffeebadc0ffeebadc")));
        project->waitForSyncToFinish();

        REQUIRE(project->errorModel()->count() >= 1);

        // HEAD unchanged
        const auto afterOid = QQuickGit::GitRepository::headCommitOid(repoPath);
        REQUIRE_FALSE(afterOid.hasError());
        CHECK(afterOid.value() == secondOid.value());
    }

    SECTION("restoreToCommit returns false while sync is in progress") {
        CHECK(project->restoreToCommit(firstOid.value()));
        // While sync is running, a second call should be rejected
        CHECK_FALSE(project->restoreToCommit(firstOid.value()));
        project->waitForSyncToFinish();
    }

    SECTION("Working directory is clean after restore") {
        CHECK(project->restoreToCommit(firstOid.value()));
        project->waitForSyncToFinish();

        repo->checkStatus();
        CHECK(repo->modifiedFileCount() == 0);
    }
}

// ---------------------------------------------------------------------------
// #1 forward-compat: unknown JSON fields must not cause parse failure
// ---------------------------------------------------------------------------
namespace {
bool injectUnknownJsonField(const QString& filePath, const QString& key, const QJsonValue& value)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) return false;
    QJsonObject root = doc.object();
    root[key] = value;

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    file.write(QJsonDocument(root).toJson());
    return true;
}
}

TEST_CASE("loadAll succeeds when entity files contain unknown JSON fields", "[cwSaveLoad]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    cave->setName(QStringLiteral("ForwardCompatCave"));
    cave->addTrip();
    cave->trip(0)->setName(QStringLiteral("ForwardCompatTrip"));

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectPath = QDir(tempDir.path()).filePath(QStringLiteral("ForwardCompat.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    const QString savedProjectFile = project->filename();

    SECTION("Unknown field in cave file loads successfully") {
        auto caveFiles = findEntityFiles(QFileInfo(savedProjectFile).absolutePath(), {QStringLiteral("*.cwcave")});
        REQUIRE(caveFiles.size() == 1);
        REQUIRE(injectUnknownJsonField(caveFiles.first(), QStringLiteral("futureFieldFromV10"), QStringLiteral("some value")));

        auto loadFuture = cwSaveLoad::loadAll(savedProjectFile);
        REQUIRE(AsyncFuture::waitForFinished(loadFuture, 10000));
        REQUIRE_FALSE(loadFuture.result().hasError());

        const auto& loadData = loadFuture.result().value();
        CHECK(loadData.region.caves.size() == 1);
        CHECK(loadData.region.caves.first().name == QStringLiteral("ForwardCompatCave"));
    }

    SECTION("Unknown field in trip file loads successfully") {
        auto tripFiles = findEntityFiles(QFileInfo(savedProjectFile).absolutePath(), {QStringLiteral("*.cwtrip")});
        REQUIRE(tripFiles.size() == 1);
        REQUIRE(injectUnknownJsonField(tripFiles.first(), QStringLiteral("futureFieldFromV10"), 42));

        auto loadFuture = cwSaveLoad::loadAll(savedProjectFile);
        REQUIRE(AsyncFuture::waitForFinished(loadFuture, 10000));
        REQUIRE_FALSE(loadFuture.result().hasError());

        const auto& loadData = loadFuture.result().value();
        CHECK(loadData.region.caves.first().trips.size() == 1);
    }

    SECTION("Unknown field in project file loads successfully") {
        auto projFiles = findEntityFiles(QFileInfo(savedProjectFile).absolutePath(), {QStringLiteral("*.cwproj")});
        REQUIRE(projFiles.size() == 1);
        REQUIRE(injectUnknownJsonField(projFiles.first(), QStringLiteral("futureFieldFromV10"), true));

        auto loadFuture = cwSaveLoad::loadAll(savedProjectFile);
        REQUIRE(AsyncFuture::waitForFinished(loadFuture, 10000));
        REQUIRE_FALSE(loadFuture.result().hasError());
    }

    SECTION("Unknown field combined with bumped version triggers warning but still loads") {
        auto caveFiles = findEntityFiles(QFileInfo(savedProjectFile).absolutePath(), {QStringLiteral("*.cwcave")});
        REQUIRE(caveFiles.size() == 1);
        REQUIRE(injectUnknownJsonField(caveFiles.first(), QStringLiteral("futureFieldFromV10"), QStringLiteral("data")));
        REQUIRE(bumpFileVersion(caveFiles.first(), cwRegionIOTask::protoVersion() + 1));

        auto loadFuture = cwSaveLoad::loadAll(savedProjectFile);
        REQUIRE(AsyncFuture::waitForFinished(loadFuture, 10000));
        REQUIRE_FALSE(loadFuture.result().hasError());

        const auto& loadData = loadFuture.result().value();
        // Data loads despite unknown field
        CHECK(loadData.region.caves.size() == 1);
        // Version warning is present
        REQUIRE(loadData.errors.size() == 1);
        CHECK(loadData.errors.first().type() == cwError::Warning);
        CHECK(loadData.errors.first().message().contains(QStringLiteral("newer version")));
    }
}

// ---------------------------------------------------------------------------
// #3 sanitizeFileName: edge cases for the dot-stripping bug fix
// ---------------------------------------------------------------------------
TEST_CASE("sanitizeFileName dot-stripping edge cases", "[cwSaveLoad]") {
    SECTION("Leading dot only: .X -> X") {
        CHECK(cwSaveLoad::sanitizeFileName(QStringLiteral(".X")) == QStringLiteral("X"));
    }

    SECTION("Trailing dot only: X. -> X") {
        CHECK(cwSaveLoad::sanitizeFileName(QStringLiteral("X.")) == QStringLiteral("X"));
    }

    SECTION("Both ends: ..foo.. -> foo") {
        CHECK(cwSaveLoad::sanitizeFileName(QStringLiteral("..foo..")) == QStringLiteral("foo"));
    }

    SECTION("All dots: ... -> untitled") {
        CHECK(cwSaveLoad::sanitizeFileName(QStringLiteral("...")) == QStringLiteral("untitled"));
    }

    SECTION("Single leading dot: .hidden -> hidden") {
        CHECK(cwSaveLoad::sanitizeFileName(QStringLiteral(".hidden")) == QStringLiteral("hidden"));
    }

    SECTION("Interior dots preserved: a.b.c -> a.b.c") {
        CHECK(cwSaveLoad::sanitizeFileName(QStringLiteral("a.b.c")) == QStringLiteral("a.b.c"));
    }
}

// ---------------------------------------------------------------------------
// #4 cave unit round-trip and missing-field default
// ---------------------------------------------------------------------------
namespace {
bool removeJsonField(const QString& filePath, const QString& key)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) return false;
    QJsonObject root = doc.object();
    root.remove(key);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    file.write(QJsonDocument(root).toJson());
    return true;
}
}

TEST_CASE("Cave lengthUnit and depthUnit survive save-load round-trip", "[cwSaveLoad]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    auto region = project->cavingRegion();
    region->addCave();
    auto cave = region->cave(0);
    cave->setName(QStringLiteral("UnitTestCave"));
    cave->length()->setUnit(cwUnits::Meters);
    cave->depth()->setUnit(cwUnits::Feet);

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectPath = QDir(tempDir.path()).filePath(QStringLiteral("UnitRoundTrip.cwproj"));
    REQUIRE(project->saveAs(projectPath));
    rootData->futureManagerModel()->waitForFinished();
    project->waitSaveToFinish();

    const QString savedProjectFile = project->filename();

    SECTION("Units round-trip correctly") {
        auto loadFuture = cwSaveLoad::loadAll(savedProjectFile);
        REQUIRE(AsyncFuture::waitForFinished(loadFuture, 10000));
        REQUIRE_FALSE(loadFuture.result().hasError());

        const auto& caves = loadFuture.result().value().region.caves;
        REQUIRE(caves.size() == 1);
        CHECK(caves.first().lengthUnit == cwUnits::Meters);
        CHECK(caves.first().depthUnit == cwUnits::Feet);
    }

    SECTION("Missing unit fields default to Meters, not Inches") {
        auto caveFiles = findEntityFiles(QFileInfo(savedProjectFile).absolutePath(), {QStringLiteral("*.cwcave")});
        REQUIRE(caveFiles.size() == 1);
        REQUIRE(removeJsonField(caveFiles.first(), QStringLiteral("lengthUnit")));
        REQUIRE(removeJsonField(caveFiles.first(), QStringLiteral("depthUnit")));

        auto loadFuture = cwSaveLoad::loadAll(savedProjectFile);
        REQUIRE(AsyncFuture::waitForFinished(loadFuture, 10000));
        REQUIRE_FALSE(loadFuture.result().hasError());

        const auto& caves = loadFuture.result().value().region.caves;
        REQUIRE(caves.size() == 1);
        CHECK(caves.first().lengthUnit == cwUnits::Meters);
        CHECK(caves.first().depthUnit == cwUnits::Meters);
    }
}

// ---------------------------------------------------------------------------
// #9 proto3 optional int32 page=0 survives JSON round-trip
// ---------------------------------------------------------------------------
TEST_CASE("Image page=0 preserved in proto3 JSON round-trip", "[cwSaveLoad]") {
    CavewhereProto::Image protoImage;
    protoImage.set_page(0);

    CHECK(protoImage.has_page());
    CHECK(protoImage.page() == 0);

    google::protobuf::util::JsonPrintOptions printOpts;
    printOpts.add_whitespace = true;
    std::string json;
    auto printStatus = google::protobuf::util::MessageToJsonString(protoImage, &json, printOpts);
    REQUIRE(printStatus.ok());

    // JSON must contain "page": 0 — not omit it
    CHECK(json.find("\"page\"") != std::string::npos);

    // Round-trip: parse the JSON back
    CavewhereProto::Image reloaded;
    auto parseStatus = google::protobuf::util::JsonStringToMessage(json, &reloaded);
    REQUIRE(parseStatus.ok());
    CHECK(reloaded.has_page());
    CHECK(reloaded.page() == 0);
}

TEST_CASE("setName rejects name that collides with sibling sanitized name", "[NameCollision]")
{
    SECTION("cwCave: sibling collision rejected")
    {
        cwCavingRegion region;
        auto* cave1 = new cwCave();
        cave1->setName("Big Cave");
        region.addCave(cave1);

        auto* cave2 = new cwCave();
        cave2->setName("Other Cave");
        region.addCave(cave2);

        // Exact same name should be rejected (sanitizes identically)
        cave2->setName("Big Cave");
        CHECK(cave2->name() == "Other Cave"); // rejected

        // "Big Cave!" sanitizes to "Big Cave_" which differs from "Big Cave" — allowed
        cave2->setName("Big Cave!");
        CHECK(cave2->name() == "Big Cave!"); // accepted — different sanitized name
    }

    SECTION("cwTrip: sibling collision rejected")
    {
        cwCavingRegion region;
        auto* cave = new cwCave();
        cave->setName("Test Cave");
        region.addCave(cave);

        auto* trip1 = new cwTrip();
        trip1->setName("Trip A");
        cave->addTrip(trip1);

        auto* trip2 = new cwTrip();
        trip2->setName("Trip B");
        cave->addTrip(trip2);

        // Exact collision — rejected
        trip2->setName("Trip A");
        CHECK(trip2->name() == "Trip B"); // rejected — "Trip A" already exists
    }

    SECTION("cwNote: sibling collision rejected")
    {
        cwCavingRegion region;
        auto* cave = new cwCave();
        cave->setName("Test Cave");
        region.addCave(cave);

        auto* trip = new cwTrip();
        trip->setName("Trip 1");
        cave->addTrip(trip);

        auto* note1 = new cwNote();
        note1->setName("scan.png");
        auto* note2 = new cwNote();
        note2->setName("photo.png");
        trip->notes()->addNotes({note1, note2});

        note2->setName("scan.png");
        CHECK(note2->name() == "photo.png"); // rejected
    }

    SECTION("cwNote: empty string rejected")
    {
        cwNote note;
        note.setName("original");
        note.setName("");
        CHECK(note.name() == "original");
    }
}

TEST_CASE("setName allows rename when no parent is set", "[NameCollision]")
{
    SECTION("cwCave without parent region")
    {
        cwCave cave;
        cave.setName("First");
        cave.setName("Second");
        CHECK(cave.name() == "Second"); // no parent — always allowed
    }

    SECTION("cwTrip without parent cave")
    {
        cwTrip trip;
        trip.setName("Trip A");
        trip.setName("Trip B");
        CHECK(trip.name() == "Trip B"); // no parent — always allowed
    }
}

TEST_CASE("insertCave auto-renames when sanitized name collides", "[NameCollision]")
{
    cwCavingRegion region;

    auto* cave1 = new cwCave();
    cave1->setName("Big Cave");
    region.addCave(cave1);

    // Insert a cave whose name collides
    auto* cave2 = new cwCave();
    cave2->setName("Big Cave");
    region.addCave(cave2);

    CHECK(cave1->name() == "Big Cave");
    CHECK(cave2->name() == "Big Cave 2"); // auto-renamed
}

TEST_CASE("auto-rename suffix increments correctly", "[NameCollision]")
{
    cwCavingRegion region;

    auto* cave1 = new cwCave();
    cave1->setName("Big Cave");
    region.addCave(cave1);

    auto* cave2 = new cwCave();
    cave2->setName("Big Cave");
    region.addCave(cave2);
    CHECK(cave2->name() == "Big Cave 2");

    auto* cave3 = new cwCave();
    cave3->setName("Big Cave");
    region.addCave(cave3);
    CHECK(cave3->name() == "Big Cave 3");
}

TEST_CASE("auto-rename skips already-taken suffixes", "[NameCollision]")
{
    cwCavingRegion region;

    auto* cave1 = new cwCave();
    cave1->setName("Big Cave");
    region.addCave(cave1);

    auto* cave2 = new cwCave();
    cave2->setName("Big Cave 2");
    region.addCave(cave2);

    // Now insert another "Big Cave" — should skip 2 and go to 3
    auto* cave3 = new cwCave();
    cave3->setName("Big Cave");
    region.addCave(cave3);
    CHECK(cave3->name() == "Big Cave 3");
}

TEST_CASE("importing caves with names that differ only in forbidden chars", "[NameCollision]")
{
    cwCavingRegion region;

    auto* cave1 = new cwCave();
    cave1->setName("Big Cave!");
    auto* cave2 = new cwCave();
    cave2->setName("Big Cave?");

    // Both sanitize to "Big Cave_". When added together, second should be renamed.
    region.addCaves({cave1, cave2});

    CHECK(cwSaveLoad::sanitizeFileName(cave1->name()) != cwSaveLoad::sanitizeFileName(cave2->name()));
    CHECK(region.caveCount() == 2);
}

TEST_CASE("insertTrip auto-renames when sanitized name collides", "[NameCollision]")
{
    cwCavingRegion region;
    auto* cave = new cwCave();
    cave->setName("Test Cave");
    region.addCave(cave);

    auto* trip1 = new cwTrip();
    trip1->setName("Survey");
    cave->addTrip(trip1);

    auto* trip2 = new cwTrip();
    trip2->setName("Survey");
    cave->addTrip(trip2);

    CHECK(trip1->name() == "Survey");
    CHECK(trip2->name() == "Survey 2");
}

TEST_CASE("setName after parenting rejects collision even when insertCave accepted", "[NameCollision]")
{
    cwCavingRegion region;

    auto* cave1 = new cwCave();
    cave1->setName("Alpha");
    region.addCave(cave1);

    auto* cave2 = new cwCave();
    cave2->setName("Beta");
    region.addCave(cave2); // accepted — "Beta" is unique

    // Now try to rename cave2 to "Alpha" via setName — should be rejected
    cave2->setName("Alpha");
    CHECK(cave2->name() == "Beta");
}

TEST_CASE("validateName returns correct rejection reasons", "[NameCollision]")
{
    SECTION("cwCave: empty name")
    {
        cwCave cave;
        CHECK_FALSE(cave.validateName("").isEmpty());
    }

    SECTION("cwCave: name with forbidden chars")
    {
        cwCave cave;
        const QString result = cave.validateName("Test?Name");
        CHECK_FALSE(result.isEmpty());
        CHECK(result.contains("Test_Name"));
    }

    SECTION("cwCave: collision with sibling")
    {
        cwCavingRegion region;
        auto* cave1 = new cwCave();
        cave1->setName("Existing Cave");
        region.addCave(cave1);

        auto* cave2 = new cwCave();
        cave2->setName("Other");
        region.addCave(cave2);

        const QString result = cave2->validateName("Existing Cave");
        CHECK_FALSE(result.isEmpty());
        CHECK(result.contains("already exists"));
    }

    SECTION("cwCave: valid name returns empty")
    {
        cwCavingRegion region;
        auto* cave = new cwCave();
        cave->setName("Alpha");
        region.addCave(cave);

        CHECK(cave->validateName("Beta").isEmpty());
    }

    SECTION("cwTrip: collision with sibling")
    {
        cwCavingRegion region;
        auto* cave = new cwCave();
        cave->setName("Cave");
        region.addCave(cave);

        auto* trip1 = new cwTrip();
        trip1->setName("Survey Day 1");
        cave->addTrip(trip1);

        auto* trip2 = new cwTrip();
        trip2->setName("Other");
        cave->addTrip(trip2);

        CHECK_FALSE(trip2->validateName("Survey Day 1").isEmpty());
    }

    SECTION("cwNote: collision with sibling")
    {
        cwCavingRegion region;
        auto* cave = new cwCave();
        cave->setName("Cave");
        region.addCave(cave);

        auto* trip = new cwTrip();
        trip->setName("Trip");
        cave->addTrip(trip);

        auto* note1 = new cwNote();
        note1->setName("scan.png");
        auto* note2 = new cwNote();
        note2->setName("photo.png");
        trip->notes()->addNotes({note1, note2});

        CHECK_FALSE(note2->validateName("scan.png").isEmpty());
        CHECK(note1->validateName("newname.png").isEmpty());
    }
}

TEST_CASE("deduplicateName utility", "[NameCollision]")
{
    SECTION("no collision")
    {
        cwSanitizedNameSet names;
        names.insert("Alpha");
        names.insert("Beta");
        CHECK(names.deduplicateName("Gamma") == "Gamma");
    }

    SECTION("collision produces suffix 2")
    {
        cwSanitizedNameSet names;
        names.insert("Big Cave");
        CHECK(names.deduplicateName("Big Cave") == "Big Cave 2");
    }

    SECTION("suffix 2 taken, produces suffix 3")
    {
        cwSanitizedNameSet names;
        names.insert("Big Cave");
        names.insert("Big Cave 2");
        CHECK(names.deduplicateName("Big Cave") == "Big Cave 3");
    }
}

TEST_CASE("load-time collision repair renames duplicates", "[NameCollision]")
{
    cwSaveLoad::ProjectLoadData loadData;

    cwCaveData cave1;
    cave1.name = "Big Cave";
    cave1.id = QUuid::createUuid();

    cwCaveData cave2;
    cave2.name = "Big Cave";
    cave2.id = QUuid::createUuid();

    loadData.region.caves = {cave1, cave2};

    cwSaveLoad::repairNameCollisions(loadData);

    CHECK(loadData.region.caves[0].name == "Big Cave");
    CHECK(loadData.region.caves[1].name == "Big Cave 2");
}

TEST_CASE("deep UUID regeneration for copied cave subtree", "[NameCollision]")
{
    cwSaveLoad::ProjectLoadData loadData;

    QUuid sharedCaveId = QUuid::createUuid();
    QUuid sharedTripId = QUuid::createUuid();
    QUuid sharedNoteId = QUuid::createUuid();
    QUuid sharedScrapId = QUuid::createUuid();
    QUuid sharedStationId = QUuid::createUuid();
    QUuid sharedLeadId = QUuid::createUuid();
    QUuid sharedLiDARNoteId = QUuid::createUuid();
    QUuid sharedLiDARStationId = QUuid::createUuid();

    auto makeCave = [&]() {
        cwCaveData cave;
        cave.name = "Copied Cave";
        cave.id = sharedCaveId;

        cwTripData trip;
        trip.name = "Trip 1";
        trip.id = sharedTripId;

        cwNoteData note;
        note.name = "scan.png";
        note.id = sharedNoteId;

        cwScrapData scrap;
        scrap.id = sharedScrapId;
        cwNoteStation station;
        station.setId(sharedStationId);
        scrap.stations.append(station);
        cwLead lead;
        lead.setId(sharedLeadId);
        scrap.leads.append(lead);

        note.scraps.append(scrap);
        trip.noteModel.notes.append(note);

        cwNoteLiDARData lidarNote;
        lidarNote.name = "scan.e57";
        lidarNote.id = sharedLiDARNoteId;
        cwNoteLiDARStation lidarStation;
        lidarStation.setId(sharedLiDARStationId);
        lidarNote.stations.append(lidarStation);
        trip.noteLiDARModel.notes.append(lidarNote);

        cave.trips.append(trip);
        return cave;
    };

    loadData.region.caves.append(makeCave());
    loadData.region.caves.append(makeCave()); // filesystem copy — same UUIDs

    cwSaveLoad::repairTopLevelIds(loadData);

    auto& caves = loadData.region.caves;

    // First cave keeps original UUIDs
    CHECK(caves[0].id == sharedCaveId);
    CHECK(caves[0].trips[0].id == sharedTripId);
    CHECK(caves[0].trips[0].noteModel.notes[0].id == sharedNoteId);
    CHECK(caves[0].trips[0].noteModel.notes[0].scraps[0].id == sharedScrapId);
    CHECK(caves[0].trips[0].noteModel.notes[0].scraps[0].stations[0].id() == sharedStationId);
    CHECK(caves[0].trips[0].noteModel.notes[0].scraps[0].leads[0].id() == sharedLeadId);
    CHECK(caves[0].trips[0].noteLiDARModel.notes[0].id == sharedLiDARNoteId);
    CHECK(caves[0].trips[0].noteLiDARModel.notes[0].stations[0].id() == sharedLiDARStationId);

    // Second cave has all new UUIDs
    CHECK(caves[1].id != sharedCaveId);
    CHECK(caves[1].trips[0].id != sharedTripId);
    CHECK(caves[1].trips[0].noteModel.notes[0].id != sharedNoteId);
    CHECK(caves[1].trips[0].noteModel.notes[0].scraps[0].id != sharedScrapId);
    CHECK(caves[1].trips[0].noteModel.notes[0].scraps[0].stations[0].id() != sharedStationId);
    CHECK(caves[1].trips[0].noteModel.notes[0].scraps[0].leads[0].id() != sharedLeadId);
    CHECK(caves[1].trips[0].noteLiDARModel.notes[0].id != sharedLiDARNoteId);
    CHECK(caves[1].trips[0].noteLiDARModel.notes[0].stations[0].id() != sharedLiDARStationId);

    // All UUIDs differ between the two caves
    CHECK(caves[0].id != caves[1].id);
    CHECK(caves[0].trips[0].id != caves[1].trips[0].id);
}

TEST_CASE("deep UUID regeneration for copied trip subtree", "[NameCollision]")
{
    cwSaveLoad::ProjectLoadData loadData;

    QUuid tripId = QUuid::createUuid();
    QUuid noteId = QUuid::createUuid();
    QUuid scrapId = QUuid::createUuid();
    QUuid stationId = QUuid::createUuid();

    auto makeTrip = [&]() {
        cwTripData trip;
        trip.name = "Survey";
        trip.id = tripId;

        cwNoteData note;
        note.name = "scan.png";
        note.id = noteId;

        cwScrapData scrap;
        scrap.id = scrapId;
        cwNoteStation station;
        station.setId(stationId);
        scrap.stations.append(station);
        note.scraps.append(scrap);

        trip.noteModel.notes.append(note);
        return trip;
    };

    cwCaveData cave1;
    cave1.name = "Cave A";
    cave1.id = QUuid::createUuid();
    cave1.trips.append(makeTrip());

    cwCaveData cave2;
    cave2.name = "Cave B";
    cave2.id = QUuid::createUuid();
    cave2.trips.append(makeTrip()); // same trip UUID copied to another cave

    loadData.region.caves = {cave1, cave2};

    cwSaveLoad::repairTopLevelIds(loadData);

    auto& caves = loadData.region.caves;

    // First cave's trip keeps original UUIDs
    CHECK(caves[0].trips[0].id == tripId);
    CHECK(caves[0].trips[0].noteModel.notes[0].id == noteId);
    CHECK(caves[0].trips[0].noteModel.notes[0].scraps[0].id == scrapId);
    CHECK(caves[0].trips[0].noteModel.notes[0].scraps[0].stations[0].id() == stationId);

    // Second cave's trip has all new UUIDs
    CHECK(caves[1].trips[0].id != tripId);
    CHECK(caves[1].trips[0].noteModel.notes[0].id != noteId);
    CHECK(caves[1].trips[0].noteModel.notes[0].scraps[0].id != scrapId);
    CHECK(caves[1].trips[0].noteModel.notes[0].scraps[0].stations[0].id() != stationId);
}

TEST_CASE("deep UUID regeneration for copied note subtree", "[NameCollision]")
{
    cwSaveLoad::ProjectLoadData loadData;

    QUuid noteId = QUuid::createUuid();
    QUuid scrapId = QUuid::createUuid();
    QUuid stationId = QUuid::createUuid();
    QUuid leadId = QUuid::createUuid();

    auto makeNote = [&]() {
        cwNoteData note;
        note.name = "scan.png";
        note.id = noteId;

        cwScrapData scrap;
        scrap.id = scrapId;
        cwNoteStation station;
        station.setId(stationId);
        scrap.stations.append(station);
        cwLead lead;
        lead.setId(leadId);
        scrap.leads.append(lead);
        note.scraps.append(scrap);

        return note;
    };

    cwCaveData cave;
    cave.name = "Test Cave";
    cave.id = QUuid::createUuid();

    cwTripData trip1;
    trip1.name = "Trip 1";
    trip1.id = QUuid::createUuid();
    trip1.noteModel.notes.append(makeNote());

    cwTripData trip2;
    trip2.name = "Trip 2";
    trip2.id = QUuid::createUuid();
    trip2.noteModel.notes.append(makeNote()); // same note UUID copied to another trip

    cave.trips = {trip1, trip2};
    loadData.region.caves.append(cave);

    cwSaveLoad::repairTopLevelIds(loadData);

    auto& trips = loadData.region.caves[0].trips;

    // First trip's note keeps original UUIDs
    CHECK(trips[0].noteModel.notes[0].id == noteId);
    CHECK(trips[0].noteModel.notes[0].scraps[0].id == scrapId);
    CHECK(trips[0].noteModel.notes[0].scraps[0].stations[0].id() == stationId);
    CHECK(trips[0].noteModel.notes[0].scraps[0].leads[0].id() == leadId);

    // Second trip's note has all new UUIDs
    CHECK(trips[1].noteModel.notes[0].id != noteId);
    CHECK(trips[1].noteModel.notes[0].scraps[0].id != scrapId);
    CHECK(trips[1].noteModel.notes[0].scraps[0].stations[0].id() != stationId);
    CHECK(trips[1].noteModel.notes[0].scraps[0].leads[0].id() != leadId);
}

TEST_CASE("deep UUID regeneration for copied LiDAR note", "[NameCollision]")
{
    cwSaveLoad::ProjectLoadData loadData;

    QUuid lidarNoteId = QUuid::createUuid();
    QUuid lidarStationId = QUuid::createUuid();

    auto makeLiDARNote = [&]() {
        cwNoteLiDARData note;
        note.name = "scan.e57";
        note.id = lidarNoteId;

        cwNoteLiDARStation station;
        station.setId(lidarStationId);
        note.stations.append(station);

        return note;
    };

    cwCaveData cave;
    cave.name = "Test Cave";
    cave.id = QUuid::createUuid();

    cwTripData trip1;
    trip1.name = "Trip 1";
    trip1.id = QUuid::createUuid();
    trip1.noteLiDARModel.notes.append(makeLiDARNote());

    cwTripData trip2;
    trip2.name = "Trip 2";
    trip2.id = QUuid::createUuid();
    trip2.noteLiDARModel.notes.append(makeLiDARNote()); // same LiDAR note UUID copied

    cave.trips = {trip1, trip2};
    loadData.region.caves.append(cave);

    cwSaveLoad::repairTopLevelIds(loadData);

    auto& trips = loadData.region.caves[0].trips;

    // First trip's LiDAR note keeps original UUIDs
    CHECK(trips[0].noteLiDARModel.notes[0].id == lidarNoteId);
    CHECK(trips[0].noteLiDARModel.notes[0].stations[0].id() == lidarStationId);

    // Second trip's LiDAR note has all new UUIDs
    CHECK(trips[1].noteLiDARModel.notes[0].id != lidarNoteId);
    CHECK(trips[1].noteLiDARModel.notes[0].stations[0].id() != lidarStationId);
}

TEST_CASE("save registers SaveFuture with futureManagerModel", "[cwSaveLoad]") {
    auto root = std::make_unique<cwRootData>();
    auto project = root->project();
    auto region = project->cavingRegion();
    auto* futureModel = root->futureManagerModel();

    root->account()->setName(QStringLiteral("Test User"));
    root->account()->setEmail(QStringLiteral("test@example.com"));

    region->addCave();
    region->cave(0)->addTrip();

    futureModel->waitForFinished();
    project->waitSaveToFinish();

    QSignalSpy allFinishedSpy(futureModel, &cwFutureManagerModel::allFinished);

    SECTION("saveAs registers the save future") {
        QTemporaryDir tempDir;
        REQUIRE(tempDir.isValid());
        const QString projectPath = QDir(tempDir.path()).filePath(QStringLiteral("future-test/future-test.cwproj"));

        REQUIRE(project->saveAs(projectPath));

        CHECK(futureModel->rowCount() > 0);

        project->waitSaveToFinish();
        futureModel->waitForFinished();

        CHECK(futureModel->isEmpty());
        CHECK(allFinishedSpy.count() >= 1);
    }

    SECTION("save registers the save future") {
        QTemporaryDir tempDir;
        REQUIRE(tempDir.isValid());
        const QString projectPath = QDir(tempDir.path()).filePath(QStringLiteral("future-test2/future-test2.cwproj"));

        REQUIRE(project->saveAs(projectPath));
        project->waitSaveToFinish();
        futureModel->waitForFinished();

        allFinishedSpy.clear();

        region->cave(0)->setName("Modified Cave");
        project->waitSaveToFinish();

        REQUIRE(project->save());

        CHECK(futureModel->rowCount() > 0);

        project->waitSaveToFinish();
        futureModel->waitForFinished();

        CHECK(futureModel->isEmpty());
        CHECK(allFinishedSpy.count() >= 1);
    }

    SECTION("saveAs bundled registers the save future") {
        QTemporaryDir tempDir;
        REQUIRE(tempDir.isValid());

        // First save as cwproj so project is non-temporary
        const QString cwprojPath = QDir(tempDir.path()).filePath(QStringLiteral("future-bundle/future-bundle.cwproj"));
        REQUIRE(project->saveAs(cwprojPath));
        project->waitSaveToFinish();
        futureModel->waitForFinished();

        allFinishedSpy.clear();

        // Now saveAs bundled .cw
        const QString bundlePath = QDir(tempDir.path()).filePath(QStringLiteral("future-bundle.cw"));
        REQUIRE(project->saveAs(bundlePath));

        CHECK(futureModel->rowCount() > 0);

        project->waitSaveToFinish();
        futureModel->waitForFinished();

        CHECK(futureModel->isEmpty());
        CHECK(allFinishedSpy.count() >= 1);
    }
}
