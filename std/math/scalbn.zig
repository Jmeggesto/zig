const assert = @import("std").debug.assert;

pub fn scalbn(x: f64, n: i64) -> f64 {

  var y: f64 = x;

  var u: u64 = u64((0x3ff+n)<<52);
  return y * (*(&f64)(&u));

}

pub fn testScalbn(){
  @setFnTest(this);

  const c = @cImport(@cInclude("math.h"));

  assert(scalbn(1.0, 10) == c.scalbn(1.0, 10));

}
