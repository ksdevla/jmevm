/*
 * Java's floating point <-> string conversions.
 *
 * toString (JDK 19+ specification, which the expected test output follows):
 * the shortest decimal that rounds back to the value; when one digit is
 * enough, the closest decimal of one or two digits. It is formatted as a
 * plain number for 1e-3 <= |v| < 1e7 and as d.dddEn otherwise, always with a
 * digit after the point.
 *
 * Digits come from the C library's correctly rounded printf/strtod, which
 * give the same results on every platform.
 *
 * Known gap: when the nearest p-digit decimal falls just outside the
 * rounding interval at a power of two while another p-digit decimal is
 * inside it, this prints p+1 digits where Java prints p.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vm.h"

/* Splits "d.ddde[+-]x" into digits without trailing zeros and exponent. */
static void split(const char *s, char *digits, int *exp10) {
	int n = 0;
	for (; *s && *s != 'e'; s++)
		if (*s >= '0' && *s <= '9') digits[n++] = *s;
	while (n > 1 && digits[n - 1] == '0') n--;
	digits[n] = '\0';
	*exp10 = *s == 'e' ? atoi(s + 1) : 0;
}

static int round_trips(const char *s, double v, int is_float) {
	if (is_float) return strtof(s, NULL) == (float)v;
	return strtod(s, NULL) == v;
}

/* Shortest digits of positive finite v; returns the decimal exponent. */
static int shortest(double v, int is_float, char *digits) {
	char buf[40];
	int max = is_float ? 9 : 17;
	int p = 1;
	for (; p < max; p++) {
		snprintf(buf, sizeof buf, "%.*e", p - 1, v);
		if (round_trips(buf, v, is_float)) break;
	}
	if (p == max) snprintf(buf, sizeof buf, "%.*e", p - 1, v);
	if (p == 1) {
		/* One digit is enough: the closest of the one- and two-digit
		 * decimals wins, and the nearest two-digit one is at least as close. */
		char two[40];
		snprintf(two, sizeof two, "%.1e", v);
		if (round_trips(two, v, is_float)) memcpy(buf, two, sizeof two);
	}
	int e;
	split(buf, digits, &e);
	return e;
}

static void format(int negative, const char *digits, int e, char *out) {
	int n = (int)strlen(digits);
	char *o = out;
	if (negative) *o++ = '-';
	if (e >= -3 && e < 7) {
		if (e < 0) {
			*o++ = '0';
			*o++ = '.';
			for (int i = -1; i > e; i--) *o++ = '0';
			memcpy(o, digits, (size_t)n);
			o += n;
		} else {
			for (int i = 0; i <= e; i++) *o++ = i < n ? digits[i] : '0';
			*o++ = '.';
			if (n > e + 1) {
				memcpy(o, digits + e + 1, (size_t)(n - e - 1));
				o += n - e - 1;
			} else {
				*o++ = '0';
			}
		}
		*o = '\0';
		return;
	}
	*o++ = digits[0];
	*o++ = '.';
	if (n > 1) {
		memcpy(o, digits + 1, (size_t)(n - 1));
		o += n - 1;
	} else {
		*o++ = '0';
	}
	sprintf(o, "E%d", e);
}

static void to_java_string(double v, int is_float, char *out) {
	if (v != v) {
		strcpy(out, "NaN");
		return;
	}
	if (isinf(v)) {
		strcpy(out, v > 0 ? "Infinity" : "-Infinity");
		return;
	}
	if (v == 0) {
		strcpy(out, signbit(v) ? "-0.0" : "0.0");
		return;
	}
	char digits[24];
	int e = shortest(fabs(v), is_float, digits);
	format(v < 0, digits, e, out);
}

void double_to_java_string(double d, char *buf) {
	to_java_string(d, 0, buf);
}

void float_to_java_string(float f, char *buf) {
	to_java_string(f, 1, buf);
}

/*
 * Checks Java's decimal grammar (surrounding whitespace, sign, NaN,
 * Infinity, digits with an optional point and exponent, f/F/d/D suffix) and
 * copies the number without the suffix into out. Hexadecimal floats are not
 * accepted yet.
 */
static int clean_number(const char *s, char *out, size_t cap) {
	while (*s && (unsigned char)*s <= ' ') s++;
	size_t len = strlen(s);
	while (len > 0 && (unsigned char)s[len - 1] <= ' ') len--;
	if (len == 0 || len + 1 > cap) return 0;
	memcpy(out, s, len);
	out[len] = '\0';

	const char *p = out;
	if (*p == '+' || *p == '-') p++;
	if (strcmp(p, "NaN") == 0 || strcmp(p, "Infinity") == 0) return 1;
	int digits = 0;
	while (*p >= '0' && *p <= '9') p++, digits++;
	if (*p == '.') {
		p++;
		while (*p >= '0' && *p <= '9') p++, digits++;
	}
	if (digits == 0) return 0;
	if (*p == 'e' || *p == 'E') {
		p++;
		if (*p == '+' || *p == '-') p++;
		if (!(*p >= '0' && *p <= '9')) return 0;
		while (*p >= '0' && *p <= '9') p++;
	}
	if (*p == 'f' || *p == 'F' || *p == 'd' || *p == 'D') {
		*(char *)p = '\0';
		p++;
		return *p == '\0';
	}
	return *p == '\0';
}

int java_parse_double(const char *s, double *out) {
	char buf[512];
	if (!clean_number(s, buf, sizeof buf)) return 0;
	const char *p = buf + (buf[0] == '+' || buf[0] == '-');
	int negative = buf[0] == '-';
	if (strcmp(p, "NaN") == 0) *out = NAN;
	else if (strcmp(p, "Infinity") == 0) *out = negative ? -INFINITY : INFINITY;
	else *out = strtod(buf, NULL);
	return 1;
}

int java_parse_float(const char *s, float *out) {
	char buf[512];
	if (!clean_number(s, buf, sizeof buf)) return 0;
	const char *p = buf + (buf[0] == '+' || buf[0] == '-');
	int negative = buf[0] == '-';
	if (strcmp(p, "NaN") == 0) *out = NAN;
	else if (strcmp(p, "Infinity") == 0) *out = negative ? -INFINITY : INFINITY;
	else *out = strtof(buf, NULL);
	return 1;
}
