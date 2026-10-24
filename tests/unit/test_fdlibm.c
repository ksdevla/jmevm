#include <stdio.h>
#include <string.h>

#include "unit.h"
#include "vm.h"

/* x, sin(x), cos(x), tan(x) as IEEE bits, from java.lang.StrictMath (fdlibm)
 * on JDK 21. The results must match bit for bit. */
static const unsigned long long cases[][4] = {
	{0x3fe0000000000000ULL, 0x3fdeaee8744b05f0ULL, 0x3fec1528065b7d50ULL, 0x3fe17b4f5bf3474aULL}, /* 0.5 */
	{0x3ff0000000000000ULL, 0x3feaed548f090ceeULL, 0x3fe14a280fb5068cULL, 0x3ff8eb245cbee3a6ULL}, /* 1.0 */
	{0x4000000000000000ULL, 0x3fed18f6ead1b446ULL, 0xbfdaa22657537205ULL, 0xc0017af62e0950f8ULL}, /* 2.0 */
	{0x4008000000000000ULL, 0x3fc210386db6d55bULL, 0xbfefae04be85e5d2ULL, 0xbfc23ef71254b86fULL}, /* 3.0 */
	{0x4024000000000000ULL, 0xbfe1689ef5f34f52ULL, 0xbfead9ac890c6b1fULL, 0x3fe4bf5f34be3782ULL}, /* 10.0 */
	{0x4059000000000000ULL, 0xbfe03425b78c4db8ULL, 0x3feb981dbf665fdfULL, 0xbfe2ca74d62b5d38ULL}, /* 100.0 */
	{0x40f86a0000000000ULL, 0x3fa24daa9c527e96ULL, 0xbfeffac3841b3da7ULL, 0xbfa250a9d503313dULL}, /* 1e5 */
	{0x41cdcd6500000000ULL, 0x3fe1778cae83c69bULL, 0x3feacff8c7364234ULL, 0x3fe4d8b249e3dba7ULL}, /* 1e9 */
	{0x4480f0cf064dd592ULL, 0xbfeb453ab76bf397ULL, 0x3fe0be2cef01c8f4ULL, 0xbffa0f79c1b6b258ULL}, /* 1e22 */
	{0xbfe6666666666666ULL, 0xbfe49d6e694619b8ULL, 0x3fe87996529f9d93ULL, 0xbfeaf406c2fc78aeULL}, /* -0.7 */
	{0x3ddb7cdfd9d7bdbbULL, 0x3ddb7cdfd9d7bdbbULL, 0x3ff0000000000000ULL, 0x3ddb7cdfd9d7bdbbULL}, /* 1e-10 */
	{0x3fe91eb851eb851fULL, 0x3fe69e4fd79ac743ULL, 0x3fe6a2ecb934b59aULL, 0x3feff97aa571156eULL}, /* 0.785 */
	{0x3ff921fb54442d18ULL, 0x3ff0000000000000ULL, 0x3c91a62633145c07ULL, 0x434d02967c31cdb5ULL}, /* pi/2 */
	{0x400921fb54442d18ULL, 0x3ca1a62633145c07ULL, 0xbff0000000000000ULL, 0xbca1a62633145c07ULL}, /* pi */
	{0x7e37e43c8800759cULL, 0xbfea2c16b010e385ULL, 0xbfe2699022adc4c1ULL, 0x3ff6be411f37ac77ULL}, /* 1e300 */
	{0xc0fe240c9fbe76c9ULL, 0x3feff50e60ab53f9ULL, 0x3faa74d27c41b22aULL, 0x403353a85fe8d6d5ULL}, /* -123456.789 */
	{0x3fe595810624dd2fULL, 0x3fe3fbf5dcfbee76ULL, 0x3fe8fe1c310e58a8ULL, 0x3fe9965d147d8180ULL}, /* 0.6745 */
	{0x3e3e1094d643f784ULL, 0x3e3e1094d643f784ULL, 0x3ff0000000000000ULL, 0x3e3e1094d643f784ULL}, /* 7e-9 */
	{0x430c6bf526340000ULL, 0x3feb76f88136cebaULL, 0xbfe06c154609d33eULL, 0xbffac23600a95be4ULL}, /* 1e15 */
	{0x4016000000000000ULL, 0xbfe693c94e0ab057ULL, 0x3fe6ad6c3c07d448ULL, 0xbfefdbd31615b07aULL}, /* 5.5 */
};

static double from_bits(unsigned long long b) {
	double d;
	memcpy(&d, &b, 8);
	return d;
}

static unsigned long long to_bits(double d) {
	unsigned long long b;
	memcpy(&b, &d, 8);
	return b;
}

void test_fdlibm_matches_strictmath(void) {
	static double (*const fns[3])(double) = {fdlibm_sin, fdlibm_cos, fdlibm_tan};
	static const char *names[3] = {"sin", "cos", "tan"};
	for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
		double x = from_bits(cases[i][0]);
		for (int f = 0; f < 3; f++) {
			unsigned long long got = to_bits(fns[f](x));
			if (got != cases[i][f + 1]) {
				char what[160];
				snprintf(what, sizeof what, "%s(%.17g) = %016llx, StrictMath gives %016llx", names[f], x, got,
						cases[i][f + 1]);
				unit_fail(__FILE__, __LINE__, what);
			}
		}
	}
	/* NaN and infinities give NaN */
	CHECK(fdlibm_sin(from_bits(0x7ff0000000000000ULL)) != fdlibm_sin(from_bits(0x7ff0000000000000ULL)));
	CHECK(fdlibm_cos(from_bits(0x7ff8000000000000ULL)) != fdlibm_cos(from_bits(0x7ff8000000000000ULL)));
	CHECK(to_bits(fdlibm_sin(-0.0)) == 0x8000000000000000ULL);
}
