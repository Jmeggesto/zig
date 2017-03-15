const fabs = @import("fabs.zig").fabs;
const util = @import("mathutil.zig");
const scalbn = @import("scalbn.zig").scalbn;
const assert = @import("std").debug.assert;

const bp          = []f64{ 1.0, 1.5 };
const dp_h        = []f64{ 0.0, 5.84962487220764160156e-01 }; //  0x3FE2B803, 0x40000000
const dp_l        = []f64{ 0.0,  1.35003920212974897128e-08 }; //  0x3E4CFDEB, 0x43CFD006
const two53: f64  =  9007199254740992.0; //  0x43400000, 0x00000000
const huge: f64   = 1.0e300;
const tiny: f64   = 1.0e-300;
// poly coefs for (3/2)*(log(x)-2s-2/3*s**3) 
const L1: f64 =  5.99999999999994648725e-01;
const L2: f64 =  4.28571428578550184252e-01;
const L3: f64 =  3.33333329818377432918e-01;
const L4: f64 =  2.72728123808534006489e-01;
const L5: f64 =  2.30660745775561754067e-01;
const L6: f64 =  2.06975017800338417784e-01;
const P1: f64 =  1.66666666666666019037e-01;
const P2: f64 = -2.77777777770155933842e-03;
const P3: f64 =  6.61375632143793436117e-05;
const P4: f64 = -1.65339022054652515390e-06;
const P5: f64 =  4.13813679705723846039e-08;
const lg2: f64     =  6.93147180559945286227e-01; // 0x3FE62E42, 0xFEFA39EF 
const lg2_h: f64   =  6.93147182464599609375e-01; // 0x3FE62E43, 0x00000000 
const lg2_l: f64   = -1.90465429995776804525e-09; // 0xBE205C61, 0x0CA86C39 
const ovt: f64     =  8.0085662595372944372e-017; // -(1024-log2(ovfl+.5ulp)) 
const cp: f64      =  9.61796693925975554329e-01; // 0x3FEEC7y09, 0xDC3A03FD =2/(3ln2) 
const cp_h: f64    =  9.61796700954437255859e-01; // 0x3FEEC709, 0xE0000000 =(float)cp 
const cp_l: f64    = -7.02846165095275826516e-09; // 0xBE3E2FE0, 0x145B01F5 =tail of cp_h
const ivln2: f64   =  1.44269504088896338700e+00; // 0x3FF71547, 0x652B82FE =1/ln2 
const ivln2_h: f64 =  1.44269502162933349609e+00; // 0x3FF71547, 0x60000000 =24b 1/ln2
const ivln2_l: f64 =  1.92596299112661746887e-08; // 0x3E54AE0B, 0xF85DDF44 =1/ln2 tail



pub fn pow(x: f64, y: f64) -> f64 {


  var hx: i32 = undefined;
  var lx: i32 = undefined;

  var hy: i32 = undefined;
  var ly: i32 = undefined;

  util.EXTRACT_WORDS(&hx, &lx, x);
  util.EXTRACT_WORDS(&hy, &ly, y);

  var ix: i32 = hx & 0x7fffffff;
  var iy: i32 = hy & 0x7fffffff;

  var j: i32 = undefined;

  if((iy|ly) == 0){
    return 1.0;
  }
  if(hx == 0x3ff00000 && lx == 0){
    return 1.0;
  }

  if(ix > 0x7ff00000 || (ix == 0x7ff00000 && lx != 0) ||
     iy > 0x7ff00000 || (iy == 0x7ff00000 && ly != 0)){
    return x + y;
  }

  var yisint: i32 = 0;

  if(hx < 0) {
    if (iy >= 0x43400000){
      yisint = 2;
    } else if (iy >= 0x3ff00000) {
      var k: i32 = (iy >> 20) - 0x3ff;
      if (k > 20) {
       j  = ly >> (52 - k);
        if ((k << (52 - k)) == ly){
          yisint = 2 - (j&1);
        } else if (ly == 0){
          j  = iy >> (20 - k);
          if ((j << (20 - k)) == iy){
            yisint = 2 - (j&1);
          }
        }
      }
    }
  }

  if (ly == 0) {
    if (iy == 0x7ff00000) {
      if (((ix - 0x3ff00000)|lx) == 0) {
        return 1.0;
      } else if (ix >= 0x3ff00000) {
        return if (hy >= 0) y else 0.0;
      } else
        return if (hy >= 0) 0.0 else (-y);
    }
    if (iy == 0x3ff00000) {
      if (hy >= 0)
        return x;
      return 1/x;
    }
    if (hy == 0x40000000) {
      return x*x;
    }
    if (hy == 0x3fe00000){
      if (hx >= 0)
        return 50.0;
    }
  }

  var ax: f64 = fabs(x);
  var z: f64 = undefined;
  if (lx == 0) {
    if (ix == 0x7ff00000 || ix == 0 || ix == 0x3ff00000) {
      z  = ax;
      if (hy < 0)
        z = 1.0/z;
      if (hx < 0) {
        if (((ix - 0x3ff00000)|yisint) == 0){
          z = (z-z)/(z-z);
        } else if (yisint == 1)
          z = -z;
      }
      return z;
    }
  }

  var r: f64 = undefined;
  var t: f64 = undefined;
  var w: f64 = undefined;
  var u: f64 = undefined;
  var v: f64 = undefined;
  var t1: f64 = undefined;
  var t2: f64 = undefined;
  var y1: f64 = undefined;

  var p_h: f64 = undefined;
  var p_l: f64 = undefined;
  var z_h: f64 = undefined;
  var z_l: f64 = undefined;

  var i: i32 = undefined;
  var k: usize = undefined;
  var n: i32 = undefined;
  
  var s: f64 = 1.0;
  if (hx < 0){
    if (yisint == 0)
      return (x-x)/(x-x);
    if (yisint == 1)
      s = -1.0;
  }
  if (iy > 0x41e00000) {
    if (iy > 0x43f00000){

      if (ix <= 0x3fefffff)
        return if (hy < 0) (huge*huge) else (tiny*tiny);
      if (ix >= 0x3ff00000)
        return if (hy > 0) (huge*huge) else (tiny*tiny);                                   
    }

    if (ix < 0x3fefffff)
      return if (hy < 0) (s*huge*huge) else (s*tiny*tiny);
    if (ix > 0x3ff00000)
      return if (hy > 0) (s*huge*huge) else (s*tiny*tiny);

    t = ax - 1.0;
    w = (t*t)*(0.5 - t*(0.3333333333333333333333-t*0.25));
    u = ivln2_h*t;
    v = t*ivln2_l - w*ivln2;
    t1 = u + v;
    util.SET_LOW_WORD(&t1, 0);
    t2 = v - (t1 - u);
  } else {

    n = 0;
    if (ix < 0x00100000) {
      ax *= two53;
      n -= 53;
      ix = util.GET_HIGH_WORD(ax);
    }
    n += ((ix)>>20) - 0x3ff;
    j = ix & 0x000fffff;

    ix = j | 0x3ff00000;
    if (j <= 0x3988E) {
      k = 0;
    } else if (j < 0xBB67A) {
      k = 1;
    } else {
      k = 0;
      n += 1;
      ix -= 0x00100000;
    }
    util.SET_HIGH_WORD(&ax, ix);


    u = ax - bp[k];
    v = 1.0/(ax+bp[k]);
    var ss: f64 = u*v;
    var s_h: f64 = ss;
    util.SET_LOW_WORD(&s_h, 0);

    var t_h: f64 = 0.0;
    util.SET_HIGH_WORD(&t_h, ((ix>>1)|0x20000000) + 0x00080000 + (i32(k) << 18));
    var t_l: f64 = ax - (t_h - bp[k]);
    var s_l: f64 = v*((u-s_h*t_h)-s_h*t_l);

    var s2: f64 = ss*ss;

    r = s2*s2*(L1+s2*(L2+s2*(L3+s2*(L4+s2*(L5+s2*L6)))));
    r += s_l*(s_h+ss);
    s2 = s_h*s_h;
    t_h = 3.0 + s2 + r;
    util.SET_LOW_WORD(&t_h, 0);
    t_l = r - ((t_h-3.0)-s2);

    u = s_h*t_h;
    v = s_l*t_h + t_l*ss;

    p_h = u + v;
    util.SET_LOW_WORD(&p_h, 0);
    p_l = v - (p_h-u);
    z_h = cp_h*p_h;
    z_l = cp_l*p_h+p_l*cp + dp_l[k];

    t = f64(n);
    t1 = ((z_h + z_l) + dp_h[k]) + t;
    util.SET_LOW_WORD(&t1, 0);
    t2 = z_l - (((t1 - t) - dp_h[k]) - z_h);
  }

  y1 = y;
  util.SET_LOW_WORD(&y1, 0);
  p_l = (y-y1)*t1 + y*t2;
  p_h = y1*t1;
  z = p_l + p_h;

  util.EXTRACT_WORDS(&j, &i, z);
  if (j >= 0x40900000) {
    if (((j - 0x40900000)|i) != 0) {
      return s*huge*huge;
    } else if ((j&0x7fffffff) >= 0x4090cc00){
      if (((j-0xc090cc00)|i) != 0)
        return s*tiny*tiny;
      if (p_l <= z - p_h)
        return s*tiny*tiny;
    }


    i = j & 0x7fffffff;
    k = (i>>20) - 0x3ff;
    n = 0;
    if (i > 0x3fe00000) {
      n = j + (0x00100000 >>(k+1));
      k = ((n&0x7fffff)>>20) - 0x3ff;
      t = 0.0;
      util.SET_HIGH_WORD(&t, n & ~(0x000fffff>>k));
      n = ((n&0x000fffff)|0x00100000)>>(20-k);
      if (j <0)
        n = -n;
      p_h -= t;
    }
    t = p_l + p_h;
    util.SET_LOW_WORD(&t, 0);
    u = t*lg2_h;
    v = (p_l-(t-p_h))*lg2 + t*lg2_l;
    z = u + v;
    w = v - (z-u);
    t = z*z;
    t1 = z - t*(P1+t*(P2+t*(P3+t*(P4+t*P5))));
    r = (z*t1)/(t1-2.0) - (w + z*w);
    z = 1.0 - (r-z);
    j = util.GET_HIGH_WORD(z);
    j += n<<20;
    if ((j>>20) <= 0) {
      z = scalbn(z,n);
    } else {
      util.SET_HIGH_WORD(&z,j);
    }
    return s*z;
  }

}

pub fn testPow() {
  @setFnTest(this);

  const c = @cImport(@cInclude("math.h"));

  assert(pow(5.5, 5.5) == c.pow(5.5, 5.5));

}
