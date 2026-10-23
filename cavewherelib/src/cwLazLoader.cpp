/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwLazLoader.h"

//Qt includes
#include <QByteArray>
#include <QByteArrayView>

// LAStools / LASlib
#include <LASlib/lasreader.hpp>

namespace {

// GeoTIFF key ids, as stored in VLR 34735 (LASF_Projection).
constexpr quint16 kGeographicTypeGeoKey = 2048;
constexpr quint16 kProjectedCSTypeGeoKey = 3072;
// A GeoTIFF key holds its value inline only when it points at no other tag.
constexpr quint16 kInlineTiffTagLocation = 0;
// GeoTIFF reserves both ends of the code range: 0 is undefined, 32767 says the
// CRS is spelled out in other keys rather than named by an EPSG code.
constexpr quint16 kUndefinedGeoCode = 0;
constexpr quint16 kUserDefinedGeoCode = 32767;

constexpr QByteArrayView kProjectionUserId = "LASF_Projection";
constexpr quint16 kOgcWktRecordId = 2112;

template<typename Record>
bool isOgcWktRecord(const Record& record)
{
    const qsizetype userIdLength = qstrnlen(record.user_id, sizeof(record.user_id));
    return record.record_id == kOgcWktRecordId
           && record.data != nullptr
           && QByteArrayView(record.user_id, userIdLength) == kProjectionUserId;
}

// The record is exactly record_length_after_header bytes, and some writers omit
// the trailing NUL, so the text ends at the record length or the first NUL.
QString wktFromRecord(const U8* data, qint64 length)
{
    const QByteArrayView payload(reinterpret_cast<const char*>(data), length);
    const qsizetype nulIndex = payload.indexOf('\0');
    return QString::fromLatin1(nulIndex >= 0 ? payload.first(nulIndex) : payload);
}

template<typename Record>
const Record* lastOgcWktRecord(const Record* records, U32 count)
{
    for (U32 i = count; i > 0; --i) {
        if (isOgcWktRecord(records[i - 1])) {
            return &records[i - 1];
        }
    }
    return nullptr;
}

// Matches LASlib's precedence: the last 2112 record wins, and EVLRs override VLRs.
QString ogcWkt(const LASheader& header)
{
    if (const LASevlr* evlr = lastOgcWktRecord(header.evlrs, header.number_of_extended_variable_length_records)) {
        return wktFromRecord(evlr->data, evlr->record_length_after_header);
    }
    if (const LASvlr* vlr = lastOgcWktRecord(header.vlrs, header.number_of_variable_length_records)) {
        return wktFromRecord(vlr->data, vlr->record_length_after_header);
    }
    return QString();
}

quint16 inlineGeoKeyValue(const LASheader& header, quint16 keyId)
{
    if (header.vlr_geo_keys == nullptr || header.vlr_geo_key_entries == nullptr) {
        return kUndefinedGeoCode;
    }

    const int keyCount = header.vlr_geo_keys->number_of_keys;
    for (int i = 0; i < keyCount; ++i) {
        const LASvlr_key_entry& entry = header.vlr_geo_key_entries[i];
        if (entry.key_id == keyId && entry.tiff_tag_location == kInlineTiffTagLocation) {
            return entry.value_offset;
        }
    }
    return kUndefinedGeoCode;
}

} // namespace

// Files using the modern (LAS 1.4) CRS encoding carry OGC WKT in a
// LASF_Projection/2112 (E)VLR. PROJ accepts WKT strings directly, so we pass it
// straight through. Older LAS files name their CRS with GeoTIFF
// GeoKeys instead (vlr_geo_keys / vlr_geo_key_entries); for those we read the
// projected CS code, falling back to the geographic one, and hand PROJ the
// "EPSG:<code>" form it resolves directly. A file that names its CRS neither
// way loads with an empty source CS and the transform short-circuits to
// identity; the user can supply an explicit override to handle that case.
static QString extractEmbeddedCS(const LASheader& header)
{
    const QString wkt = ogcWkt(header);
    if (!wkt.isEmpty()) {
        return wkt;
    }

    for (const quint16 keyId : {kProjectedCSTypeGeoKey, kGeographicTypeGeoKey}) {
        const quint16 code = inlineGeoKeyValue(header, keyId);
        if (code != kUndefinedGeoCode && code != kUserDefinedGeoCode) {
            return QStringLiteral("EPSG:") + QString::number(code);
        }
    }

    return QString();
}

QString cwLazLoader::resolveSourceCS(const QString& override, const LASheader& header)
{
    return override.isEmpty() ? extractEmbeddedCS(header) : override;
}

cwLazLoader::ProbeResult cwLazLoader::probeHeader(const QString& path)
{
    ProbeResult result;

    const QByteArray pathBytes = path.toUtf8();

    LASreadOpener opener;
    opener.set_file_name(pathBytes.constData(), FALSE);
    LASreader* reader = opener.open();
    if (reader == nullptr) {
        return result;
    }

    const LASheader& header = reader->header;
    result.sourceCS = extractEmbeddedCS(header);
    result.bboxMin = cwGeoPoint(header.min_x, header.min_y, header.min_z);
    result.bboxMax = cwGeoPoint(header.max_x, header.max_y, header.max_z);
    result.valid = true;

    reader->close();
    delete reader;
    return result;
}
