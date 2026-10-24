/*
 * sin, cos and tan from fdlibm 5.3 (http://www.netlib.org/fdlibm/: s_sin.c,
 * s_cos.c, s_tan.c, k_sin.c, k_cos.c, k_tan.c, e_rem_pio2.c,
 * k_rem_pio2.c), the library Java's StrictMath is specified against. Pure
 * double arithmetic, so the results are the same on every platform (the
 * build disables FMA contraction).
 *
 * Changes for jmevm: the files are merged; the __HI/__LO pointer casts are
 * replaced by memcpy helpers (no aliasing or byte-order assumptions); K&R
 * declarations and the 113-bit precision case of __kernel_rem_pio2 are
 * dropped; names are prefixed.
 *
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 *
 * k_tan.c:
 * ====================================================
 * Copyright 2004 Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */
#include <math.h>
#include <string.h>

#include "vm.h"

static i32 hi_word(double x) {
	u64 b;
	memcpy(&b, &x, 8);
	return (i32)(u32)(b >> 32);
}

static u32 lo_word(double x) {
	u64 b;
	memcpy(&b, &x, 8);
	return (u32)b;
}

static double from_words(i32 hi, u32 lo) {
	u64 b = (u64)(u32)hi << 32 | lo;
	double x;
	memcpy(&x, &b, 8);
	return x;
}

static double with_lo_zero(double x) {
	return from_words(hi_word(x), 0);
}

/* ---- k_sin.c: sin on [-pi/4, pi/4]; y is the tail of x, iy=0 if y is 0 ---- */

static const double
	S1 = -1.66666666666666324348e-01, /* 0xBFC55555, 0x55555549 */
	S2 = 8.33333333332248946124e-03,  /* 0x3F811111, 0x1110F8A6 */
	S3 = -1.98412698298579493134e-04, /* 0xBF2A01A0, 0x19C161D5 */
	S4 = 2.75573137070700676789e-06,  /* 0x3EC71DE3, 0x57B1FE7D */
	S5 = -2.50507602534068634195e-08, /* 0xBE5AE5E6, 0x8A2B9CEB */
	S6 = 1.58969099521155010221e-10;  /* 0x3DE5D93A, 0x5ACFD57C */

static double kernel_sin(double x, double y, int iy) {
	i32 ix = hi_word(x) & 0x7fffffff;
	if (ix < 0x3e400000) {         /* |x| < 2**-27 */
		if ((int)x == 0) return x; /* generate inexact */
	}
	double z = x * x;
	double v = z * x;
	double r = S2 + z * (S3 + z * (S4 + z * (S5 + z * S6)));
	if (iy == 0) return x + v * (S1 + z * r);
	return x - ((z * (0.5 * y - v * r) - y) - v * S1);
}

/* ---- k_cos.c: cos on [-pi/4, pi/4]; y is the tail of x ---- */

static const double
	C1 = 4.16666666666666019037e-02,  /* 0x3FA55555, 0x5555554C */
	C2 = -1.38888888888741095749e-03, /* 0xBF56C16C, 0x16C15177 */
	C3 = 2.48015872894767294178e-05,  /* 0x3EFA01A0, 0x19CB1590 */
	C4 = -2.75573143513906633035e-07, /* 0xBE927E4F, 0x809C52AD */
	C5 = 2.08757232129817482790e-09,  /* 0x3E21EE9E, 0xBDB4B1C4 */
	C6 = -1.13596475577881948265e-11; /* 0xBDA8FAE9, 0xBE8838D4 */

static double kernel_cos(double x, double y) {
	i32 ix = hi_word(x) & 0x7fffffff;
	if (ix < 0x3e400000) {           /* |x| < 2**-27 */
		if ((int)x == 0) return 1.0; /* generate inexact */
	}
	double z = x * x;
	double r = z * (C1 + z * (C2 + z * (C3 + z * (C4 + z * (C5 + z * C6)))));
	if (ix < 0x3FD33333) /* |x| < 0.3 */
		return 1.0 - (0.5 * z - (z * r - x * y));
	double qx;
	if (ix > 0x3fe90000) { /* x > 0.78125 */
		qx = 0.28125;
	} else {
		qx = from_words(ix - 0x00200000, 0); /* x/4 */
	}
	double hz = 0.5 * z - qx;
	double a = 1.0 - qx;
	return a - (hz - (z * r - x * y));
}

/* ---- k_tan.c: tan (iy=1) or -1/tan (iy=-1) on [-pi/4, pi/4] ---- */

static const double T[] = {
	3.33333333333334091986e-01,  /* 3FD55555, 55555563 */
	1.33333333333201242699e-01,  /* 3FC11111, 1110FE7A */
	5.39682539762260521377e-02,  /* 3FABA1BA, 1BB341FE */
	2.18694882948595424599e-02,  /* 3F9664F4, 8406D637 */
	8.86323982359930005737e-03,  /* 3F8226E3, E96E8493 */
	3.59207910759131235356e-03,  /* 3F6D6D22, C9560328 */
	1.45620945432529025516e-03,  /* 3F57DBC8, FEE08315 */
	5.88041240820264096874e-04,  /* 3F4344D8, F2F26501 */
	2.46463134818469906812e-04,  /* 3F3026F7, 1A8D1068 */
	7.81794442939557092300e-05,  /* 3F147E88, A03792A6 */
	7.14072491382608190305e-05,  /* 3F12B80F, 32F0A7E9 */
	-1.85586374855275456654e-05, /* BEF375CB, DB605373 */
	2.59073051863633712884e-05,  /* 3EFB2A70, 74BF7AD4 */
};
static const double
	pio4 = 7.85398163397448278999e-01,   /* 3FE921FB, 54442D18 */
	pio4lo = 3.06161699786838301793e-17; /* 3C81A626, 33145C07 */

static double kernel_tan(double x, double y, int iy) {
	double z, r, v, w, s;
	i32 hx = hi_word(x);
	i32 ix = hx & 0x7fffffff;
	if (ix < 0x3e300000) {  /* |x| < 2**-28 */
		if ((int)x == 0) { /* generate inexact */
			if (((ix | (i32)lo_word(x)) | (iy + 1)) == 0) return 1.0 / fabs(x);
			if (iy == 1) return x;
			/* compute -1 / (x+y) carefully */
			double a, t;
			z = w = x + y;
			z = with_lo_zero(z);
			v = y - (z - x);
			t = a = -1.0 / w;
			t = with_lo_zero(t);
			s = 1.0 + t * z;
			return t + a * (s + t * v);
		}
	}
	if (ix >= 0x3FE59428) { /* |x| >= 0.6744 */
		if (hx < 0) {
			x = -x;
			y = -y;
		}
		z = pio4 - x;
		w = pio4lo - y;
		x = z + w;
		y = 0.0;
	}
	z = x * x;
	w = z * z;
	/* x^5*(T[1]+x^2*T[2]+...) split into odd and even powers of x^4 */
	r = T[1] + w * (T[3] + w * (T[5] + w * (T[7] + w * (T[9] + w * T[11]))));
	v = z * (T[2] + w * (T[4] + w * (T[6] + w * (T[8] + w * (T[10] + w * T[12])))));
	s = z * x;
	r = y + z * (s * (r + v) + y);
	r += T[0] * s;
	w = x + r;
	if (ix >= 0x3FE59428) {
		v = (double)iy;
		return (double)(1 - ((hx >> 30) & 2)) * (v - 2.0 * (x - (w * w / (w + v) - r)));
	}
	if (iy == 1) return w;
	/* compute -1.0 / (x+r) accurately */
	double a, t;
	z = with_lo_zero(w);
	v = r - (z - x);    /* z+v = r+x */
	t = a = -1.0 / w; /* a = -1.0/w */
	t = with_lo_zero(t);
	s = 1.0 + t * z;
	return t + a * (s + t * v);
}

/* ---- k_rem_pio2.c: x rem pi/2 for large x, x given in 24-bit pieces ---- */

static const double PIo2[] = {
	1.57079625129699707031e+00, /* 0x3FF921FB, 0x40000000 */
	7.54978941586159635335e-08, /* 0x3E74442D, 0x00000000 */
	5.39030252995776476554e-15, /* 0x3CF84698, 0x80000000 */
	3.28200341580791294123e-22, /* 0x3B78CC51, 0x60000000 */
	1.27065575308067607349e-29, /* 0x39F01B83, 0x80000000 */
	1.22933308981111328932e-36, /* 0x387A2520, 0x40000000 */
	2.73370053816464559624e-44, /* 0x36E38222, 0x80000000 */
	2.16741683877804819444e-51, /* 0x3569F31D, 0x00000000 */
};
static const double
	two24 = 1.67772160000000000000e+07,  /* 0x41700000, 0x00000000 */
	twon24 = 5.96046447753906250000e-08; /* 0x3E700000, 0x00000000 */

/* prec is 2 (64 bits) as in fdlibm's own call; y gets 2 terms */
static int kernel_rem_pio2(const double *x, double *y, int e0, int nx, const i32 *ipio2) {
	static const int jk = 4; /* init_jk[2] */
	int jz, jx, jv, jp, carry, n, i, j, k, m, q0, ih;
	i32 iq[20];
	double z, fw, f[20], fq[20], q[20];

	jp = jk;

	/* determine jx, jv, q0; note that 3 > q0 */
	jx = nx - 1;
	jv = (e0 - 3) / 24;
	if (jv < 0) jv = 0;
	q0 = e0 - 24 * (jv + 1);

	/* set up f[0] to f[jx+jk] where f[jx+jk] = ipio2[jv+jk] */
	j = jv - jx;
	m = jx + jk;
	for (i = 0; i <= m; i++, j++) f[i] = (j < 0) ? 0.0 : (double)ipio2[j];

	/* compute q[0], q[1], ... q[jk] */
	for (i = 0; i <= jk; i++) {
		for (j = 0, fw = 0.0; j <= jx; j++) fw += x[j] * f[jx + i - j];
		q[i] = fw;
	}

	jz = jk;
recompute:
	/* distill q[] into iq[] reversingly */
	for (i = 0, j = jz, z = q[jz]; j > 0; i++, j--) {
		fw = (double)((i32)(twon24 * z));
		iq[i] = (i32)(z - two24 * fw);
		z = q[j - 1] + fw;
	}

	/* compute n */
	z = scalbn(z, q0);             /* actual value of z */
	z -= 8.0 * floor(z * 0.125); /* trim off integer >= 8 */
	n = (int)z;
	z -= (double)n;
	ih = 0;
	if (q0 > 0) { /* need iq[jz-1] to determine n */
		i = (iq[jz - 1] >> (24 - q0));
		n += i;
		iq[jz - 1] -= i << (24 - q0);
		ih = iq[jz - 1] >> (23 - q0);
	} else if (q0 == 0) {
		ih = iq[jz - 1] >> 23;
	} else if (z >= 0.5) {
		ih = 2;
	}

	if (ih > 0) { /* q > 0.5 */
		n += 1;
		carry = 0;
		for (i = 0; i < jz; i++) { /* compute 1-q */
			j = iq[i];
			if (carry == 0) {
				if (j != 0) {
					carry = 1;
					iq[i] = 0x1000000 - j;
				}
			} else {
				iq[i] = 0xffffff - j;
			}
		}
		if (q0 > 0) { /* rare case: chance is 1 in 12 */
			switch (q0) {
			case 1: iq[jz - 1] &= 0x7fffff; break;
			case 2: iq[jz - 1] &= 0x3fffff; break;
			default: break;
			}
		}
		if (ih == 2) {
			z = 1.0 - z;
			if (carry != 0) z -= scalbn(1.0, q0);
		}
	}

	/* check if recomputation is needed */
	if (z == 0.0) {
		j = 0;
		for (i = jz - 1; i >= jk; i--) j |= iq[i];
		if (j == 0) { /* need recomputation */
			for (k = 1; iq[jk - k] == 0; k++) /* k = no. of terms needed */
				;
			for (i = jz + 1; i <= jz + k; i++) { /* add q[jz+1] to q[jz+k] */
				f[jx + i] = (double)ipio2[jv + i];
				for (j = 0, fw = 0.0; j <= jx; j++) fw += x[j] * f[jx + i - j];
				q[i] = fw;
			}
			jz += k;
			goto recompute;
		}
	}

	/* chop off zero terms */
	if (z == 0.0) {
		jz -= 1;
		q0 -= 24;
		while (iq[jz] == 0) {
			jz--;
			q0 -= 24;
		}
	} else { /* break z into 24-bit if necessary */
		z = scalbn(z, -q0);
		if (z >= two24) {
			fw = (double)((i32)(twon24 * z));
			iq[jz] = (i32)(z - two24 * fw);
			jz += 1;
			q0 += 24;
			iq[jz] = (i32)fw;
		} else {
			iq[jz] = (i32)z;
		}
	}

	/* convert integer "bit" chunk to floating-point value */
	fw = scalbn(1.0, q0);
	for (i = jz; i >= 0; i--) {
		q[i] = fw * (double)iq[i];
		fw *= twon24;
	}

	/* compute PIo2[0,...,jp]*q[jz,...,0] */
	for (i = jz; i >= 0; i--) {
		for (fw = 0.0, k = 0; k <= jp && k <= jz - i; k++) fw += PIo2[k] * q[i + k];
		fq[jz - i] = fw;
	}

	/* compress fq[] into y[] */
	fw = 0.0;
	for (i = jz; i >= 0; i--) fw += fq[i];
	y[0] = (ih == 0) ? fw : -fw;
	fw = fq[0] - fw;
	for (i = 1; i <= jz; i++) fw += fq[i];
	y[1] = (ih == 0) ? fw : -fw;
	return n & 7;
}

/* ---- e_rem_pio2.c: x rem pi/2 in y[0] + y[1] ---- */

/* 396 hex digits (476 decimal) of 2/pi */
static const i32 two_over_pi[] = {
	0xA2F983, 0x6E4E44, 0x1529FC, 0x2757D1, 0xF534DD, 0xC0DB62,
	0x95993C, 0x439041, 0xFE5163, 0xABDEBB, 0xC561B7, 0x246E3A,
	0x424DD2, 0xE00649, 0x2EEA09, 0xD1921C, 0xFE1DEB, 0x1CB129,
	0xA73EE8, 0x8235F5, 0x2EBB44, 0x84E99C, 0x7026B4, 0x5F7E41,
	0x3991D6, 0x398353, 0x39F49C, 0x845F8B, 0xBDF928, 0x3B1FF8,
	0x97FFDE, 0x05980F, 0xEF2F11, 0x8B5A0A, 0x6D1F6D, 0x367ECF,
	0x27CB09, 0xB74F46, 0x3F669E, 0x5FEA2D, 0x7527BA, 0xC7EBE5,
	0xF17B3D, 0x0739F7, 0x8A5292, 0xEA6BFB, 0x5FB11F, 0x8D5D08,
	0x560330, 0x46FC7B, 0x6BABF0, 0xCFBC20, 0x9AF436, 0x1DA9E3,
	0x91615E, 0xE61B08, 0x659985, 0x5F14A0, 0x68408D, 0xFFD880,
	0x4D7327, 0x310606, 0x1556CA, 0x73A8C9, 0x60E27B, 0xC08C6B,
};

static const i32 npio2_hw[] = {
	0x3FF921FB, 0x400921FB, 0x4012D97C, 0x401921FB, 0x401F6A7A, 0x4022D97C,
	0x4025FDBB, 0x402921FB, 0x402C463A, 0x402F6A7A, 0x4031475C, 0x4032D97C,
	0x40346B9C, 0x4035FDBB, 0x40378FDB, 0x403921FB, 0x403AB41B, 0x403C463A,
	0x403DD85A, 0x403F6A7A, 0x40407E4C, 0x4041475C, 0x4042106C, 0x4042D97C,
	0x4043A28C, 0x40446B9C, 0x404534AC, 0x4045FDBB, 0x4046C6CB, 0x40478FDB,
	0x404858EB, 0x404921FB,
};

static const double
	invpio2 = 6.36619772367581382433e-01, /* 0x3FE45F30, 0x6DC9C883: 53 bits of 2/pi */
	pio2_1 = 1.57079632673412561417e+00,  /* 0x3FF921FB, 0x54400000: first 33 bits of pi/2 */
	pio2_1t = 6.07710050650619224932e-11, /* 0x3DD0B461, 0x1A626331: pi/2 - pio2_1 */
	pio2_2 = 6.07710050630396597660e-11,  /* 0x3DD0B461, 0x1A600000: second 33 bits */
	pio2_2t = 2.02226624879595063154e-21, /* 0x3BA3198A, 0x2E037073: pi/2 - (pio2_1+pio2_2) */
	pio2_3 = 2.02226624871116645580e-21,  /* 0x3BA3198A, 0x2E000000: third 33 bits */
	pio2_3t = 8.47842766036889956997e-32; /* 0x397B839A, 0x252049C1 */

static int rem_pio2(double x, double *y) {
	double z, w, t, r, fn;
	double tx[3];
	i32 e0, i, j, nx, n, ix, hx;

	hx = hi_word(x);
	ix = hx & 0x7fffffff;
	if (ix <= 0x3fe921fb) { /* |x| ~<= pi/4, no need for reduction */
		y[0] = x;
		y[1] = 0;
		return 0;
	}
	if (ix < 0x4002d97c) { /* |x| < 3pi/4, special case with n=+-1 */
		if (hx > 0) {
			z = x - pio2_1;
			if (ix != 0x3ff921fb) { /* 33+53 bit pi is good enough */
				y[0] = z - pio2_1t;
				y[1] = (z - y[0]) - pio2_1t;
			} else { /* near pi/2, use 33+33+53 bit pi */
				z -= pio2_2;
				y[0] = z - pio2_2t;
				y[1] = (z - y[0]) - pio2_2t;
			}
			return 1;
		}
		z = x + pio2_1;
		if (ix != 0x3ff921fb) {
			y[0] = z + pio2_1t;
			y[1] = (z - y[0]) + pio2_1t;
		} else {
			z += pio2_2;
			y[0] = z + pio2_2t;
			y[1] = (z - y[0]) + pio2_2t;
		}
		return -1;
	}
	if (ix <= 0x413921fb) { /* |x| ~<= 2^19*(pi/2), medium size */
		t = fabs(x);
		n = (i32)(t * invpio2 + 0.5);
		fn = (double)n;
		r = t - fn * pio2_1;
		w = fn * pio2_1t; /* 1st round good to 85 bits */
		if (n < 32 && ix != npio2_hw[n - 1]) {
			y[0] = r - w; /* quick check no cancellation */
		} else {
			j = ix >> 20;
			y[0] = r - w;
			i = j - ((hi_word(y[0]) >> 20) & 0x7ff);
			if (i > 16) { /* 2nd iteration needed, good to 118 bits */
				t = r;
				w = fn * pio2_2;
				r = t - w;
				w = fn * pio2_2t - ((t - r) - w);
				y[0] = r - w;
				i = j - ((hi_word(y[0]) >> 20) & 0x7ff);
				if (i > 49) { /* 3rd iteration needed, 151 bits */
					t = r;
					w = fn * pio2_3;
					r = t - w;
					w = fn * pio2_3t - ((t - r) - w);
					y[0] = r - w;
				}
			}
		}
		y[1] = (r - y[0]) - w;
		if (hx < 0) {
			y[0] = -y[0];
			y[1] = -y[1];
			return -n;
		}
		return n;
	}
	/* all other (large) arguments */
	if (ix >= 0x7ff00000) { /* inf or NaN */
		y[0] = y[1] = x - x;
		return 0;
	}
	/* z = scalbn(|x|, ilogb(x)-23) */
	e0 = (ix >> 20) - 1046;
	z = from_words(ix - (e0 << 20), lo_word(x));
	for (i = 0; i < 2; i++) {
		tx[i] = (double)((i32)z);
		z = (z - tx[i]) * two24;
	}
	tx[2] = z;
	nx = 3;
	while (tx[nx - 1] == 0.0) nx--; /* skip zero terms */
	n = kernel_rem_pio2(tx, y, e0, nx, two_over_pi);
	if (hx < 0) {
		y[0] = -y[0];
		y[1] = -y[1];
		return -n;
	}
	return n;
}

/* ---- s_sin.c, s_cos.c, s_tan.c ---- */

double fdlibm_sin(double x) {
	double y[2];
	i32 ix = hi_word(x) & 0x7fffffff;
	if (ix <= 0x3fe921fb) return kernel_sin(x, 0.0, 0); /* |x| ~< pi/4 */
	if (ix >= 0x7ff00000) return x - x;                   /* Inf or NaN */
	switch (rem_pio2(x, y) & 3) {
	case 0: return kernel_sin(y[0], y[1], 1);
	case 1: return kernel_cos(y[0], y[1]);
	case 2: return -kernel_sin(y[0], y[1], 1);
	default: return -kernel_cos(y[0], y[1]);
	}
}

double fdlibm_cos(double x) {
	double y[2];
	i32 ix = hi_word(x) & 0x7fffffff;
	if (ix <= 0x3fe921fb) return kernel_cos(x, 0.0);
	if (ix >= 0x7ff00000) return x - x;
	switch (rem_pio2(x, y) & 3) {
	case 0: return kernel_cos(y[0], y[1]);
	case 1: return -kernel_sin(y[0], y[1], 1);
	case 2: return -kernel_cos(y[0], y[1]);
	default: return kernel_sin(y[0], y[1], 1);
	}
}

double fdlibm_tan(double x) {
	double y[2];
	i32 ix = hi_word(x) & 0x7fffffff;
	if (ix <= 0x3fe921fb) return kernel_tan(x, 0.0, 1);
	if (ix >= 0x7ff00000) return x - x;
	int n = rem_pio2(x, y);
	return kernel_tan(y[0], y[1], 1 - ((n & 1) << 1)); /* 1: n even, -1: n odd */
}
