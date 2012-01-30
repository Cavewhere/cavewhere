.pragma library

/**
  This will fix number of a number of decimals, if there isn't decimals, this
  will return a whole number

  This returns a string
  */
function fixed(number, fixed) {
    return new Number(number.toFixed(fixed)).toString();
}
