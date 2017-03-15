const assert = @import("std").debug.assert;


pub fn EXTRACT_WORDS(hi: &i32, lo: &i32, d: f64) {

  var u: u64 = *(&u64)(&d);
  var shifted: u64 = u >> 32;
  *hi = *(&i32)(&shifted);
  *lo = *(&i32)(&u);
}

pub fn GET_HIGH_WORD(d: f64) -> i32 {

  var u: u64 = *(&u64)(&d);
  u >>= 32;
  return i32(u);
}
pub fn SET_LOW_WORD(d: &f64, v: i32) {
  var u: u64 = *((&u64)(d));
  u &= 0xffffffff00000000;
  u |= u32(v);
  *d = *(&f64)(&u);
}
pub fn SET_HIGH_WORD(d: &f64, v: i32){
  var u: u64 = *((&u64)(d));
  u &= 0xffffffff;
  u |= u64(v) << 32;
  *d = *(&f64)(&u);
}

pub fn testHighWord() {
  @setFnTest(this);

  var d: f64 = 1234.5678;
  var i: i32 = GET_HIGH_WORD(d);
  assert(i == 0x40934a45);

}
pub fn testSetHigh() {
  @setFnTest(this);

  var x: f64 = 1234.5678;
  SET_HIGH_WORD(&x, 0x12345678);
  var i: i32 = GET_HIGH_WORD(x);
  assert(i == 0x12345678);
}

pub fn testExtraction() {

  @setFnTest(this);

  var x: f64 = 10.075;
  var i: i32 = undefined;
  var j: i32 = undefined;

  EXTRACT_WORDS(&i, &j, x);
  assert(i == 0x40242666 && j == 0x66666666);
}

