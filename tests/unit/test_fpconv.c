#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "unit.h"
#include "vm.h"

static void check_double(double v, const char *want) {
	char buf[40];
	double_to_java_string(v, buf);
	if (strcmp(buf, want) != 0) {
		char what[128];
		snprintf(what, sizeof what, "Double.toString gave %s, expected %s", buf, want);
		unit_fail(__FILE__, __LINE__, what);
	}
}

static void check_float(float v, const char *want) {
	char buf[40];
	float_to_java_string(v, buf);
	if (strcmp(buf, want) != 0) {
		char what[128];
		snprintf(what, sizeof what, "Float.toString gave %s, expected %s", buf, want);
		unit_fail(__FILE__, __LINE__, what);
	}
}

/* Expected strings are what JDK 21 prints. */
void test_fpconv_double_to_string(void) {
	volatile double tenth = 0.1, fifth = 0.2;
	check_double(1.0, "1.0");
	check_double(100.0, "100.0");
	check_double(0.5, "0.5");
	check_double(1234567.0, "1234567.0");
	check_double(1e7, "1.0E7");
	check_double(1e-3, "0.001");
	check_double(2e-3, "0.002");
	check_double(9.999e-4, "9.999E-4");
	check_double(123456789.0, "1.23456789E8");
	check_double(tenth + fifth, "0.30000000000000004");
	check_double(1.0 / 3, "0.3333333333333333");
	check_double(1e21, "1.0E21");
	check_double(1e23, "1.0E23");
	check_double(DBL_MAX, "1.7976931348623157E308");
	check_double(4.9e-324, "4.9E-324");
	check_double(-0.0, "-0.0");
	check_double(0.0, "0.0");
	check_double(-1.5, "-1.5");
	check_double(NAN, "NaN");
	check_double(-INFINITY, "-Infinity");
	check_double(9.223372036854776E18, "9.223372036854776E18");
}

void test_fpconv_float_to_string(void) {
	volatile float tenth = 0.1f, fifth = 0.2f;
	check_float(1.0f, "1.0");
	check_float(1.0e-4f, "1.0E-4");
	check_float(123456.79f, "123456.79");
	check_float(FLT_MAX, "3.4028235E38");
	check_float(1.4e-45f, "1.4E-45");
	check_float(tenth + fifth, "0.3");
	check_float(1e10f, "1.0E10");
	check_float(16777216.0f, "1.6777216E7");
	check_float(1.0f / 3, "0.33333334");
	check_float(-9.223372E18f, "-9.223372E18");
}

void test_fpconv_parse(void) {
	double d = 0;
	float f = 0;
	CHECK(java_parse_double("1.5e3", &d) && d == 1500.0);
	CHECK(java_parse_double("  -2.25  ", &d) && d == -2.25);
	CHECK(java_parse_double("+.5", &d) && d == 0.5);
	CHECK(java_parse_double("5.", &d) && d == 5.0);
	CHECK(java_parse_double("1.5d", &d) && d == 1.5);
	CHECK(java_parse_double("NaN", &d) && d != d);
	CHECK(java_parse_double("-Infinity", &d) && d == -INFINITY);
	CHECK(java_parse_double("0.1", &d) && d == 0.1);
	CHECK(!java_parse_double("1.2.3", &d));
	CHECK(!java_parse_double("", &d));
	CHECK(!java_parse_double(".", &d));
	CHECK(!java_parse_double("1e", &d));
	CHECK(!java_parse_double("e5", &d));
	CHECK(!java_parse_double("1.5ff", &d));
	CHECK(java_parse_float("3.4028235E38", &f) && f == FLT_MAX);
	/* the smallest subnormal float, not flushed to zero */
	CHECK(java_parse_float("1.4e-45", &f) && f > 0 && f < 1e-44f);
	CHECK(!java_parse_float("abc", &f));
}
