.pragma library

/**
  This will fix number of a number of decimals, if there isn't decimals, this
  will return a whole number

  This returns a string
  */
function fixed(number, fixed) {
    return new Number(number.toFixed(fixed)).toString();
}

/**
  Escapes the HTML metacharacters in text so a RichText item renders it
  literally. Use this on anything the user typed — a cave named A<b>B would
  otherwise be read as markup. Only needed where the surrounding text really is
  markup; a label that can be told PlainText should be told that instead.

  @param text - The plain text to escape
  @return string - text with &, < and > replaced by entities
  */
function escapeHtml(text) {
    return String(text).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
}

/**
  Formats a WGS84 pair as one comma-separated string, latitude first, in the
  order the rest of the app displays them. Degrees are degrees, so neither
  carries a unit. Callers pass their own precision: a projection parameter and a
  point the user placed are read to different resolutions.

  @param latitude - degrees north
  @param longitude - degrees east
  @param precision - decimal places, the same for both
  @return string - "36.123456, -84.089012"
  */
function formatLatLon(latitude, longitude, precision) {
    return "%1, %2".arg(latitude.toFixed(precision)).arg(longitude.toFixed(precision))
}

/**
  This will map MouseArea mouseX and mouseY to global coordinates

  @param mouseArea - The mouse area, that the mouse event came from
  @return Qt.point() - Returns the mouse event in global coordinates
  */
function mousePositionToGlobal(mouseArea) {
    var globalPoint = mouseArea.mapToItem(null, mouseArea.mouseX, mouseArea.mouseY)
    return Qt.point(globalPoint.x, globalPoint.y)
}
