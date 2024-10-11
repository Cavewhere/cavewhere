.pragma library

/**
  This will take the dot product of two Qt.point. The point is treated like a vector

  @param v1 - vector 1 (Qt.point)
  @param v2 - vector 2 (Qt.point)
  @return dot product of v1 and v2
  */
function dot(v1, v2) {
    return v1.x * v2.x + v1.y * v2.y;
}

/**
  This will find the length of Qt.point. It will treat the the point as a vector

  @param v - A vector (Qt.point)
  @return The length of v
*/
function length(v) {
    return Math.sqrt(v.x * v.x + v.y * v.y);
}

/**
  This will find angle between v1 and v2

  @param v1 - vector 1 (Qt.point)
  @param v2 - vector 2 (Qt.point)
  @return The angle in degrees
  */
function angleBetween(v1, v2) {
    var angleInRadians = Math.acos(dot(v1, v2) / (length(v1) * length(v2)))
    var radiansToDegrees = 180.0 / Math.PI;
    return angleInRadians * radiansToDegrees;
}

/**
  This will find the cross product between v1 and v2

  @param v1 - vector 1 (Qt.point)
  @param v2 - vector 2 (Qt.point)

  @return The cross product
  */
function crossProduct(v1, v2) {
    return v1.x * v2.y - v1.y * v2.x;
}

