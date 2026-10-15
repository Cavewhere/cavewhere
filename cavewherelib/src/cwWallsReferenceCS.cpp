/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwWallsReferenceCS.h"

//Survex includes
#include "img.h"

//Std includes
#include <cstdlib>

namespace {

//! The img datum for a Walls datum name.
//!
//! img.c matches Compass' spellings exactly and Walls spells several datums
//! differently, so the ones survex's own Walls reader special-cases are
//! repeated here (survex/src/datain.c). Walls carries a handful of regional
//! NAD27 realizations; they differ in how they transform to WGS84, not in which
//! CRS a coordinate given in them belongs to, so all of them name NAD27.
img_datum wallsDatum(const QString& name)
{
    const QByteArray latin1 = name.toLatin1();
    const img_datum compassDatum = img_parse_compass_datum_string(latin1.constData(),
                                                                  static_cast<size_t>(latin1.size()));
    if (compassDatum != img_DATUM_UNKNOWN) {
        return compassDatum;
    }

    if (name.startsWith(QStringLiteral("NAD27"))) {
        return img_DATUM_NAD27;
    }
    if (name == QStringLiteral("NAD83")) {
        return img_DATUM_NAD83;
    }
    if (name == QStringLiteral("Geodetic Datum `49")) {
        return img_DATUM_NZGD49;
    }
    if (name == QStringLiteral("Hu-Tzu-Shan")) {
        return img_DATUM_HUTZUSHAN1950;
    }
    return img_DATUM_UNKNOWN;
}

} // namespace

WallsReferenceCS wallsReferenceCS(const dewalls::GeoReferencePtr& reference)
{
    WallsReferenceCS referenceCS;

    if (reference.isNull()) {
        return referenceCS;
    }

    const QString datumName = reference->datumName.trimmed();
    const img_datum datum = wallsDatum(datumName);
    if (datum == img_DATUM_UNKNOWN) {
        referenceCS.unmappedDatum = datumName;
        return referenceCS;
    }

    //dewalls signs the zone the way img wants it: positive north, negative
    //south. img returns null for a zone of 0 — what a .REF with no zone gives
    //us — for the polar UPS zones, and for a zone outside the datum's area.
    if (char* utm = img_compass_utm_proj_str(datum, reference->zone)) {
        referenceCS.rect = QString::fromLatin1(utm);
        std::free(utm);
    }

    //Every datum img knows has a geodetic CRS; only img_DATUM_UNKNOWN, which
    //returned above, has none.
    referenceCS.geo = QStringLiteral("EPSG:%1").arg(img_compass_longlat_epsg_code(datum));

    return referenceCS;
}
