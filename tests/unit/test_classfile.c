#include <string.h>

#include "classfile.h"
#include "unit.h"

/* Byte writer for hand-made class files. */
typedef struct W {
	u8 b[1024];
	size_t n;
} W;

static void w1(W *w, u32 v) { w->b[w->n++] = (u8)v; }
static void w2(W *w, u32 v) { w1(w, v >> 8); w1(w, v); }
static void w4(W *w, u32 v) { w2(w, v >> 16); w2(w, v); }
static void wutf(W *w, const char *s) {
	w1(w, CP_UTF8);
	w2(w, (u32)strlen(s));
	for (; *s; s++) w1(w, (u8)*s);
}

/* Offsets of bytes the negative tests change. */
typedef struct Layout {
	size_t major, this_class, method_access;
} Layout;

/*
 *   public class T from T.java {
 *       static final int x = 42;          // ConstantValue
 *       public static int f() { return 42; }  // line 7
 *   }
 * plus a long constant to check the two-slot rule.
 */
static void build(W *w, Layout *at) {
	w->n = 0;
	w4(w, 0xCAFEBABE);
	w2(w, 0);
	at->major = w->n;
	w2(w, 52);
	w2(w, 17);                    /* cp_count */
	wutf(w, "T");                 /* 1 */
	w1(w, CP_CLASS); w2(w, 1);    /* 2 */
	wutf(w, "java/lang/Object");  /* 3 */
	w1(w, CP_CLASS); w2(w, 3);    /* 4 */
	wutf(w, "x");                 /* 5 */
	wutf(w, "I");                 /* 6 */
	wutf(w, "f");                 /* 7 */
	wutf(w, "()I");               /* 8 */
	wutf(w, "Code");              /* 9 */
	w1(w, CP_LONG); w4(w, 0x01234567); w4(w, 0x89ABCDEF); /* 10, 11 */
	wutf(w, "ConstantValue");     /* 12 */
	w1(w, CP_INTEGER); w4(w, 42); /* 13 */
	wutf(w, "LineNumberTable");   /* 14 */
	wutf(w, "SourceFile");        /* 15 */
	wutf(w, "T.java");            /* 16 */

	w2(w, ACC_PUBLIC | ACC_SUPER);
	at->this_class = w->n;
	w2(w, 2);
	w2(w, 4);
	w2(w, 0);                     /* interfaces */

	w2(w, 1);                     /* fields */
	w2(w, ACC_STATIC | ACC_FINAL);
	w2(w, 5); w2(w, 6);
	w2(w, 1);
	w2(w, 12); w4(w, 2); w2(w, 13);

	w2(w, 1);                     /* methods */
	at->method_access = w->n;
	w2(w, ACC_PUBLIC | ACC_STATIC);
	w2(w, 7); w2(w, 8);
	w2(w, 1);
	w2(w, 9); w4(w, 27);          /* Code */
	w2(w, 1); w2(w, 0);           /* max_stack, max_locals */
	w4(w, 3);
	w1(w, 0x10); w1(w, 42); w1(w, 0xAC); /* bipush 42; ireturn */
	w2(w, 0);                     /* handlers */
	w2(w, 1);
	w2(w, 14); w4(w, 6); w2(w, 1); w2(w, 0); w2(w, 7);

	w2(w, 1);                     /* class attributes */
	w2(w, 15); w4(w, 2); w2(w, 16);
}

static JmeStatus parse_status(const W *w, size_t len) {
	ClassFile *cf;
	JmeStatus st = classfile_parse(w->b, len, &cf, NULL);
	classfile_free(cf);
	return st;
}

void test_classfile_minimal(void) {
	W w;
	Layout at;
	build(&w, &at);
	ClassFile *cf;
	const char *why = "unset";
	CHECK_EQ(classfile_parse(w.b, w.n, &cf, &why), JME_OK);
	CHECK(why == NULL);
	if (cf == NULL) return;
	CHECK_EQ(cf->major, 52);
	CHECK_EQ(cf->cp_count, 17);
	CHECK(utf8_eq(cf->name, "T"));
	CHECK(utf8_eq(cf->super_name, "java/lang/Object"));
	CHECK(utf8_eq(cf->source_file, "T.java"));
	CHECK_EQ(cf->cp[10].tag, CP_LONG);
	CHECK(cf->cp[10].u.bits64 == 0x0123456789ABCDEFull);
	CHECK_EQ(cf->cp[11].tag, 0);
	CHECK_EQ(cf->field_count, 1);
	CHECK(utf8_eq(cf->fields[0].name, "x"));
	CHECK(utf8_eq(cf->fields[0].desc, "I"));
	CHECK_EQ(cf->fields[0].const_index, 13);
	CHECK_EQ(cf->method_count, 1);
	const MethodInfo *m = &cf->methods[0];
	CHECK(utf8_eq(m->name, "f"));
	CHECK(utf8_eq(m->desc, "()I"));
	CHECK(m->code != NULL);
	if (m->code != NULL) {
		CHECK_EQ(m->code->max_stack, 1);
		CHECK_EQ(m->code->code_len, 3);
		CHECK_EQ(m->code->code[2], 0xAC);
		CHECK_EQ(m->code->line_count, 1);
		CHECK_EQ(m->code->lines[0].line, 7);
	}
	/* The ClassFile keeps its own copy of the bytes. */
	memset(w.b, 0, sizeof w.b);
	CHECK(utf8_eq(cf->name, "T"));
	classfile_free(cf);
}

void test_classfile_truncated(void) {
	W w;
	Layout at;
	build(&w, &at);
	for (size_t n = 0; n < w.n; n++) {
		JmeStatus st = parse_status(&w, n);
		if (st != JME_E_FORMAT) {
			CHECK_EQ(n, -1); /* reports the failing length */
			break;
		}
	}
}

void test_classfile_trailing_bytes(void) {
	W w;
	Layout at;
	build(&w, &at);
	w.b[w.n] = 0;
	CHECK_EQ(parse_status(&w, w.n + 1), JME_E_FORMAT);
}

void test_classfile_versions(void) {
	W w;
	Layout at;
	build(&w, &at);
	static const struct { u16 major; JmeStatus want; } cases[] = {
		{44, JME_E_UNSUPPORTED}, {45, JME_OK}, {49, JME_OK}, {52, JME_OK}, {53, JME_E_UNSUPPORTED},
	};
	for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
		w.b[at.major] = (u8)(cases[i].major >> 8);
		w.b[at.major + 1] = (u8)cases[i].major;
		CHECK_EQ(parse_status(&w, w.n), cases[i].want);
	}
	build(&w, &at);
	w.b[0] = 0xCB;
	CHECK_EQ(parse_status(&w, w.n), JME_E_FORMAT);
}

void test_classfile_bad_references(void) {
	W w;
	Layout at;
	/* this_class pointing at a Utf8 instead of a Class */
	build(&w, &at);
	w.b[at.this_class + 1] = 1;
	CHECK_EQ(parse_status(&w, w.n), JME_E_FORMAT);
	/* this_class pointing past the constant pool */
	w.b[at.this_class + 1] = 200;
	CHECK_EQ(parse_status(&w, w.n), JME_E_FORMAT);
	/* this_class pointing at the unusable half of the long */
	w.b[at.this_class + 1] = 11;
	CHECK_EQ(parse_status(&w, w.n), JME_E_FORMAT);
}

void test_classfile_method_bodies(void) {
	W w;
	Layout at;
	build(&w, &at);
	/* abstract with a Code attribute */
	w.b[at.method_access] = ACC_ABSTRACT >> 8;
	CHECK_EQ(parse_status(&w, w.n), JME_E_FORMAT);
	/* native with a Code attribute */
	w.b[at.method_access] = ACC_NATIVE >> 8;
	CHECK_EQ(parse_status(&w, w.n), JME_E_FORMAT);
}

/*
 * Changes every byte to a few other values. Each result must be a clean
 * status; a crash or a sanitizer report is the failure this test looks for.
 */
void test_classfile_mutations(void) {
	W w;
	Layout at;
	build(&w, &at);
	size_t ok = 0;
	for (size_t i = 0; i < w.n; i++) {
		u8 orig = w.b[i];
		const u8 values[] = {0x00, 0xFF, (u8)(orig ^ 0x01), (u8)(orig + 1), 0x80};
		for (size_t v = 0; v < sizeof values; v++) {
			if (values[v] == orig) continue;
			w.b[i] = values[v];
			ClassFile *cf;
			JmeStatus st = classfile_parse(w.b, w.n, &cf, NULL);
			CHECK(st == JME_OK || st == JME_E_FORMAT || st == JME_E_UNSUPPORTED);
			CHECK((st == JME_OK) == (cf != NULL));
			if (st == JME_OK) ok++;
			classfile_free(cf);
		}
		w.b[i] = orig;
	}
	/* Some changes are harmless (flags, constant values); most are not. */
	CHECK(ok > 0);
}
