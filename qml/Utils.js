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
  This will map MouseArea mouseX and mouseY to global coordinates

  @param mouseArea - The mouse area, that the mouse event came from
  @return Qt.point() - Returns the mouse event in global coordinates
  */
function mousePositionToGlobal(mouseArea) {
    var globalPoint = mouseArea.mapToItem(null, mouseArea.mouseX, mouseArea.mouseY)
    return Qt.point(globalPoint.x, globalPoint.y)
}
