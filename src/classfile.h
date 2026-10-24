/*
 * Class file parser (JVMS chapter 4, versions 45.0 to 52.x).
 *
 * Parsing is purely structural: every index is checked against the constant
 * pool and every length against the input, so malformed bytes give
 * JME_E_FORMAT and never a crash. Bytecode itself is checked later, when a
 * method is prepared for the interpreter.
 *
 * The ClassFile owns a copy of the input; all Utf8 values point into it.
 * Float and double constants are kept as raw IEEE bits, so no host floating
 * point is involved.
 */
#ifndef JMEVM_CLASSFILE_H
#define JMEVM_CLASSFILE_H

#include "base.h"
#include "pool.h"

enum {
	CP_UTF8 = 1,
	CP_INTEGER = 3,
	CP_FLOAT = 4,
	CP_LONG = 5,
	CP_DOUBLE = 6,
	CP_CLASS = 7,
	CP_STRING = 8,
	CP_FIELDREF = 9,
	CP_METHODREF = 10,
	CP_INTERFACE_METHODREF = 11,
	CP_NAME_AND_TYPE = 12,
	CP_METHOD_HANDLE = 15,
	CP_METHOD_TYPE = 16,
	CP_INVOKE_DYNAMIC = 18
};

enum {
	ACC_PUBLIC = 0x0001,
	ACC_PRIVATE = 0x0002,
	ACC_PROTECTED = 0x0004,
	ACC_STATIC = 0x0008,
	ACC_FINAL = 0x0010,
	ACC_SUPER = 0x0020,       /* class; ACC_SYNCHRONIZED on methods */
	ACC_SYNCHRONIZED = 0x0020,
	ACC_VOLATILE = 0x0040,
	ACC_BRIDGE = 0x0040,
	ACC_TRANSIENT = 0x0080,
	ACC_VARARGS = 0x0080,
	ACC_NATIVE = 0x0100,
	ACC_INTERFACE = 0x0200,
	ACC_ABSTRACT = 0x0400,
	ACC_STRICT = 0x0800,
	ACC_SYNTHETIC = 0x1000,
	ACC_ANNOTATION = 0x2000,
	ACC_ENUM = 0x4000
};

/* Modified UTF-8 as stored in the class file; not NUL-terminated. */
typedef struct Utf8 {
	const u8 *bytes;
	u16 len;
} Utf8;

typedef struct CpEntry {
	u8 tag; /* 0: index 0, or the unusable slot after a long or double */
	union {
		Utf8 utf8;
		u32 bits32;  /* CP_INTEGER, CP_FLOAT */
		u64 bits64;  /* CP_LONG, CP_DOUBLE */
		u16 index;   /* CP_CLASS, CP_STRING, CP_METHOD_TYPE: a CP_UTF8 */
		struct {
			u16 a;   /* refs: CP_CLASS; CP_NAME_AND_TYPE: name; CP_INVOKE_DYNAMIC: bootstrap */
			u16 b;   /* refs and CP_INVOKE_DYNAMIC: CP_NAME_AND_TYPE; CP_NAME_AND_TYPE: descriptor */
		} pair;
		struct {
			u8 kind;
			u16 ref;
		} handle;
	} u;
} CpEntry;

typedef struct ExceptionEntry {
	u16 start_pc, end_pc, handler_pc;
	u16 catch_type; /* 0 (any) or a CP_CLASS */
} ExceptionEntry;

typedef struct LineEntry {
	u16 start_pc;
	u16 line;
} LineEntry;

typedef struct CodeAttr {
	u16 max_stack;
	u16 max_locals;
	u32 code_len;
	const u8 *code;
	u16 handler_count;
	ExceptionEntry *handlers;
	u16 line_count;
	LineEntry *lines;
} CodeAttr;

typedef struct FieldInfo {
	u16 access;
	Utf8 name;
	Utf8 desc;
	u16 const_index; /* ConstantValue, or 0 */
} FieldInfo;

typedef struct MethodInfo {
	u16 access;
	Utf8 name;
	Utf8 desc;
	CodeAttr *code; /* NULL for native and abstract methods */
} MethodInfo;

typedef struct ClassFile {
	u16 minor, major;
	u16 cp_count;
	CpEntry *cp;
	u16 access;
	Utf8 name;
	Utf8 super_name;  /* len 0 and bytes NULL for java/lang/Object */
	u16 interface_count;
	Utf8 *interfaces;
	u16 field_count;
	FieldInfo *fields;
	u16 method_count;
	MethodInfo *methods;
	Utf8 source_file; /* bytes NULL when absent */
	Pool pool;
} ClassFile;

/*
 * Parses a class file. On success *out owns a copy of data. On failure *why
 * (if not NULL) gets a short static description of the first problem.
 */
JmeStatus classfile_parse(const u8 *data, size_t len, ClassFile **out, const char **why);
void classfile_free(ClassFile *cf);

/* 1 when s equals the NUL-terminated ASCII string c. */
int utf8_eq(Utf8 s, const char *c);

#endif
