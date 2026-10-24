#include "classfile.h"

#include <stdlib.h>
#include <string.h>

/* Big-endian reader with a sticky error flag: reads past the end return 0. */
typedef struct Reader {
	const u8 *p;
	const u8 *end;
	int bad;
} Reader;

static u8 rd_u1(Reader *r) {
	if (r->end - r->p < 1) {
		r->bad = 1;
		return 0;
	}
	return *r->p++;
}

static u16 rd_u2(Reader *r) {
	if (r->end - r->p < 2) {
		r->bad = 1;
		r->p = r->end;
		return 0;
	}
	u16 v = (u16)(r->p[0] << 8 | r->p[1]);
	r->p += 2;
	return v;
}

static u32 rd_u4(Reader *r) {
	if (r->end - r->p < 4) {
		r->bad = 1;
		r->p = r->end;
		return 0;
	}
	u32 v = (u32)r->p[0] << 24 | (u32)r->p[1] << 16 | (u32)r->p[2] << 8 | r->p[3];
	r->p += 4;
	return v;
}

static const u8 *rd_bytes(Reader *r, u32 n) {
	if ((size_t)(r->end - r->p) < n) {
		r->bad = 1;
		r->p = r->end;
		return NULL;
	}
	const u8 *s = r->p;
	r->p += n;
	return s;
}

/* A reader over the next n bytes of r; r skips past them. */
static Reader rd_sub(Reader *r, u32 n) {
	Reader sub;
	sub.p = rd_bytes(r, n);
	sub.end = sub.p != NULL ? sub.p + n : NULL;
	sub.bad = sub.p == NULL;
	return sub;
}

int utf8_eq(Utf8 s, const char *c) {
	size_t n = strlen(c);
	return s.len == n && (n == 0 || memcmp(s.bytes, c, n) == 0);
}

typedef struct Parser {
	ClassFile *cf;
	const char *why;
} Parser;

#define FAIL(ps, msg) do { (ps)->why = (msg); return JME_E_FORMAT; } while (0)
#define TRUNCATED(ps, r) do { if ((r)->bad) FAIL(ps, "truncated"); } while (0)

static int cp_is(const ClassFile *cf, u16 i, u8 tag) {
	return i > 0 && i < cf->cp_count && cf->cp[i].tag == tag;
}

/* Reads a u2 that must index a CP_UTF8, into *out. */
static int rd_utf8_ref(Reader *r, const ClassFile *cf, Utf8 *out) {
	u16 i = rd_u2(r);
	if (r->bad || !cp_is(cf, i, CP_UTF8)) return 0;
	*out = cf->cp[i].u.utf8;
	return 1;
}

/* Name of a CP_CLASS entry. */
static Utf8 class_name(const ClassFile *cf, u16 i) {
	return cf->cp[cf->cp[i].u.index].u.utf8;
}

static JmeStatus parse_constant_pool(Parser *ps, Reader *r) {
	ClassFile *cf = ps->cf;
	cf->cp_count = rd_u2(r);
	TRUNCATED(ps, r);
	if (cf->cp_count == 0) FAIL(ps, "constant pool count is 0");
	cf->cp = pool_alloc(&cf->pool, (size_t)cf->cp_count * sizeof(CpEntry));
	if (cf->cp == NULL) return JME_E_NOMEM;

	for (u32 i = 1; i < cf->cp_count; i++) {
		CpEntry *e = &cf->cp[i];
		e->tag = rd_u1(r);
		switch (e->tag) {
		case CP_UTF8:
			e->u.utf8.len = rd_u2(r);
			e->u.utf8.bytes = rd_bytes(r, e->u.utf8.len);
			break;
		case CP_INTEGER:
		case CP_FLOAT:
			e->u.bits32 = rd_u4(r);
			break;
		case CP_LONG:
		case CP_DOUBLE: {
			u64 hi = rd_u4(r);
			e->u.bits64 = hi << 32 | rd_u4(r);
			/* Takes two slots; the second one stays tag 0. */
			if (i + 1 >= cf->cp_count) FAIL(ps, "long or double in the last constant pool slot");
			i++;
			break;
		}
		case CP_CLASS:
		case CP_STRING:
		case CP_METHOD_TYPE:
			e->u.index = rd_u2(r);
			break;
		case CP_FIELDREF:
		case CP_METHODREF:
		case CP_INTERFACE_METHODREF:
		case CP_NAME_AND_TYPE:
		case CP_INVOKE_DYNAMIC:
			e->u.pair.a = rd_u2(r);
			e->u.pair.b = rd_u2(r);
			break;
		case CP_METHOD_HANDLE:
			e->u.handle.kind = rd_u1(r);
			e->u.handle.ref = rd_u2(r);
			break;
		default:
			TRUNCATED(ps, r);
			FAIL(ps, "unknown constant pool tag");
		}
		TRUNCATED(ps, r);
		if (e->tag >= CP_METHOD_HANDLE && cf->major < 51)
			FAIL(ps, "method handle constant before class version 51");
	}

	/* Every reference between entries must point at the right kind. */
	for (u32 i = 1; i < cf->cp_count; i++) {
		const CpEntry *e = &cf->cp[i];
		switch (e->tag) {
		case CP_CLASS:
		case CP_STRING:
		case CP_METHOD_TYPE:
			if (!cp_is(cf, e->u.index, CP_UTF8)) FAIL(ps, "constant does not name a Utf8");
			break;
		case CP_FIELDREF:
		case CP_METHODREF:
		case CP_INTERFACE_METHODREF:
			if (!cp_is(cf, e->u.pair.a, CP_CLASS) || !cp_is(cf, e->u.pair.b, CP_NAME_AND_TYPE))
				FAIL(ps, "bad member reference");
			break;
		case CP_NAME_AND_TYPE:
			if (!cp_is(cf, e->u.pair.a, CP_UTF8) || !cp_is(cf, e->u.pair.b, CP_UTF8))
				FAIL(ps, "bad name and type");
			break;
		case CP_INVOKE_DYNAMIC:
			if (!cp_is(cf, e->u.pair.b, CP_NAME_AND_TYPE)) FAIL(ps, "bad invokedynamic");
			break;
		case CP_METHOD_HANDLE: {
			u8 k = e->u.handle.kind;
			u16 ref = e->u.handle.ref;
			int ok = k >= 1 && k <= 4 ? cp_is(cf, ref, CP_FIELDREF)
					: k >= 5 && k <= 9 ? cp_is(cf, ref, CP_METHODREF) || cp_is(cf, ref, CP_INTERFACE_METHODREF)
					: 0;
			if (!ok) FAIL(ps, "bad method handle");
			break;
		}
		default:
			break;
		}
	}
	return JME_OK;
}

/* Reads an attribute header: its name and a reader over its body. */
static int rd_attribute(Reader *r, const ClassFile *cf, Utf8 *name, Reader *body) {
	if (!rd_utf8_ref(r, cf, name)) return 0;
	u32 len = rd_u4(r);
	if (r->bad) return 0;
	*body = rd_sub(r, len);
	return !body->bad;
}

static JmeStatus parse_code(Parser *ps, Reader *r, CodeAttr **out) {
	ClassFile *cf = ps->cf;
	CodeAttr *c = pool_alloc(&cf->pool, sizeof(CodeAttr));
	if (c == NULL) return JME_E_NOMEM;
	c->max_stack = rd_u2(r);
	c->max_locals = rd_u2(r);
	c->code_len = rd_u4(r);
	TRUNCATED(ps, r);
	if (c->code_len == 0 || c->code_len > 65535) FAIL(ps, "bad code length");
	c->code = rd_bytes(r, c->code_len);
	c->handler_count = rd_u2(r);
	TRUNCATED(ps, r);
	c->handlers = pool_alloc(&cf->pool, (size_t)c->handler_count * sizeof(ExceptionEntry));
	if (c->handlers == NULL) return JME_E_NOMEM;
	for (u32 i = 0; i < c->handler_count; i++) {
		ExceptionEntry *h = &c->handlers[i];
		h->start_pc = rd_u2(r);
		h->end_pc = rd_u2(r);
		h->handler_pc = rd_u2(r);
		h->catch_type = rd_u2(r);
		TRUNCATED(ps, r);
		if (h->start_pc >= h->end_pc || h->end_pc > c->code_len || h->handler_pc >= c->code_len)
			FAIL(ps, "exception handler outside the code");
		if (h->catch_type != 0 && !cp_is(cf, h->catch_type, CP_CLASS))
			FAIL(ps, "exception handler type is not a class");
	}

	u16 attr_count = rd_u2(r);
	TRUNCATED(ps, r);
	for (u32 i = 0; i < attr_count; i++) {
		Utf8 name;
		Reader body;
		if (!rd_attribute(r, cf, &name, &body)) FAIL(ps, r->bad ? "truncated" : "bad attribute name");
		if (!utf8_eq(name, "LineNumberTable")) continue;
		u16 n = rd_u2(&body);
		LineEntry *lines = pool_alloc(&cf->pool, (size_t)n * sizeof(LineEntry));
		if (lines == NULL) return JME_E_NOMEM;
		for (u32 j = 0; j < n; j++) {
			lines[j].start_pc = rd_u2(&body);
			lines[j].line = rd_u2(&body);
			if (!body.bad && lines[j].start_pc >= c->code_len) FAIL(ps, "line number outside the code");
		}
		if (body.bad || body.p != body.end) FAIL(ps, "bad LineNumberTable");
		/* Several tables may appear; keep the largest, javac emits one. */
		if (n >= c->line_count) {
			c->lines = lines;
			c->line_count = n;
		}
	}
	if (r->p != r->end) FAIL(ps, "Code attribute length mismatch");
	*out = c;
	return JME_OK;
}

static JmeStatus parse_fields(Parser *ps, Reader *r) {
	ClassFile *cf = ps->cf;
	cf->field_count = rd_u2(r);
	TRUNCATED(ps, r);
	cf->fields = pool_alloc(&cf->pool, (size_t)cf->field_count * sizeof(FieldInfo));
	if (cf->fields == NULL) return JME_E_NOMEM;
	for (u32 i = 0; i < cf->field_count; i++) {
		FieldInfo *f = &cf->fields[i];
		f->access = rd_u2(r);
		if (!rd_utf8_ref(r, cf, &f->name) || !rd_utf8_ref(r, cf, &f->desc))
			FAIL(ps, r->bad ? "truncated" : "field name or descriptor is not a Utf8");
		u16 attr_count = rd_u2(r);
		TRUNCATED(ps, r);
		for (u32 j = 0; j < attr_count; j++) {
			Utf8 name;
			Reader body;
			if (!rd_attribute(r, cf, &name, &body)) FAIL(ps, r->bad ? "truncated" : "bad attribute name");
			if (!utf8_eq(name, "ConstantValue")) continue;
			u16 v = rd_u2(&body);
			if (body.bad || body.p != body.end) FAIL(ps, "bad ConstantValue");
			if (!(cp_is(cf, v, CP_INTEGER) || cp_is(cf, v, CP_FLOAT) || cp_is(cf, v, CP_LONG)
					|| cp_is(cf, v, CP_DOUBLE) || cp_is(cf, v, CP_STRING)))
				FAIL(ps, "ConstantValue is not a constant");
			f->const_index = v;
		}
	}
	return JME_OK;
}

static JmeStatus parse_methods(Parser *ps, Reader *r) {
	ClassFile *cf = ps->cf;
	cf->method_count = rd_u2(r);
	TRUNCATED(ps, r);
	cf->methods = pool_alloc(&cf->pool, (size_t)cf->method_count * sizeof(MethodInfo));
	if (cf->methods == NULL) return JME_E_NOMEM;
	for (u32 i = 0; i < cf->method_count; i++) {
		MethodInfo *m = &cf->methods[i];
		m->access = rd_u2(r);
		if (!rd_utf8_ref(r, cf, &m->name) || !rd_utf8_ref(r, cf, &m->desc))
			FAIL(ps, r->bad ? "truncated" : "method name or descriptor is not a Utf8");
		u16 attr_count = rd_u2(r);
		TRUNCATED(ps, r);
		for (u32 j = 0; j < attr_count; j++) {
			Utf8 name;
			Reader body;
			if (!rd_attribute(r, cf, &name, &body)) FAIL(ps, r->bad ? "truncated" : "bad attribute name");
			if (!utf8_eq(name, "Code")) continue;
			if (m->code != NULL) FAIL(ps, "two Code attributes");
			JmeStatus st = parse_code(ps, &body, &m->code);
			if (st != JME_OK) return st;
		}
		int bodyless = (m->access & (ACC_ABSTRACT | ACC_NATIVE)) != 0;
		if (bodyless && m->code != NULL) FAIL(ps, "abstract or native method has code");
		if (!bodyless && m->code == NULL) FAIL(ps, "method has no code");
	}
	return JME_OK;
}

static JmeStatus parse(Parser *ps, Reader *r) {
	ClassFile *cf = ps->cf;
	if (rd_u4(r) != 0xCAFEBABEu) FAIL(ps, "not a class file");
	cf->minor = rd_u2(r);
	cf->major = rd_u2(r);
	TRUNCATED(ps, r);
	if (cf->major < 45 || cf->major > 52) {
		ps->why = "class file version outside 45-52";
		return JME_E_UNSUPPORTED;
	}

	JmeStatus st = parse_constant_pool(ps, r);
	if (st != JME_OK) return st;

	cf->access = rd_u2(r);
	u16 this_class = rd_u2(r);
	u16 super_class = rd_u2(r);
	TRUNCATED(ps, r);
	if (!cp_is(cf, this_class, CP_CLASS)) FAIL(ps, "this_class is not a class");
	cf->name = class_name(cf, this_class);
	if (super_class == 0) {
		if (!utf8_eq(cf->name, "java/lang/Object")) FAIL(ps, "missing superclass");
	} else {
		if (!cp_is(cf, super_class, CP_CLASS)) FAIL(ps, "super_class is not a class");
		cf->super_name = class_name(cf, super_class);
	}

	cf->interface_count = rd_u2(r);
	TRUNCATED(ps, r);
	cf->interfaces = pool_alloc(&cf->pool, (size_t)cf->interface_count * sizeof(Utf8));
	if (cf->interfaces == NULL) return JME_E_NOMEM;
	for (u32 i = 0; i < cf->interface_count; i++) {
		u16 k = rd_u2(r);
		TRUNCATED(ps, r);
		if (!cp_is(cf, k, CP_CLASS)) FAIL(ps, "interface is not a class");
		cf->interfaces[i] = class_name(cf, k);
	}

	if ((st = parse_fields(ps, r)) != JME_OK) return st;
	if ((st = parse_methods(ps, r)) != JME_OK) return st;

	u16 attr_count = rd_u2(r);
	TRUNCATED(ps, r);
	for (u32 i = 0; i < attr_count; i++) {
		Utf8 name;
		Reader body;
		if (!rd_attribute(r, cf, &name, &body)) FAIL(ps, r->bad ? "truncated" : "bad attribute name");
		if (!utf8_eq(name, "SourceFile")) continue;
		if (!rd_utf8_ref(&body, cf, &cf->source_file) || body.p != body.end) FAIL(ps, "bad SourceFile");
	}
	if (r->p != r->end) FAIL(ps, "trailing bytes after the class");
	return JME_OK;
}

JmeStatus classfile_parse(const u8 *data, size_t len, ClassFile **out, const char **why) {
	*out = NULL;
	if (why != NULL) *why = NULL;
	if (len > UINT32_MAX) {
		if (why != NULL) *why = "class file too large";
		return JME_E_FORMAT;
	}
	ClassFile *cf = calloc(1, sizeof(ClassFile));
	if (cf == NULL) return JME_E_NOMEM;
	pool_init(&cf->pool);

	Parser ps = {cf, NULL};
	JmeStatus st;
	const u8 *copy = pool_dup(&cf->pool, data, len);
	if (copy == NULL) {
		st = JME_E_NOMEM;
	} else {
		Reader r = {copy, copy + len, 0};
		st = parse(&ps, &r);
	}
	if (st != JME_OK) {
		if (why != NULL) *why = st == JME_E_NOMEM ? "out of memory" : ps.why;
		classfile_free(cf);
		return st;
	}
	*out = cf;
	return JME_OK;
}

void classfile_free(ClassFile *cf) {
	if (cf == NULL) return;
	pool_free_all(&cf->pool);
	free(cf);
}
