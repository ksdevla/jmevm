/*
 * Structural checks run once per method before its first execution, so the
 * interpreter can trust what it reads: every instruction lies inside the
 * code, every branch and handler lands on an instruction, local variable
 * indexes are below max_locals and constant pool operands have the right
 * kind. Failures throw VerifyError.
 *
 * Not checked yet: operand stack depth and types (a full dataflow
 * verifier). Games are preverified by the CLDC toolchain.
 */
#include <stdio.h>
#include <stdlib.h>

#include "opcodes.h"
#include "vm.h"

static u32 fixed_len(u8 op) {
	if (op <= OP_DCONST_1) return 1;
	switch (op) {
	case OP_BIPUSH: case OP_LDC: case OP_RET: case OP_NEWARRAY: return 2;
	case OP_SIPUSH: case OP_LDC_W: case OP_LDC2_W: case OP_IINC: return 3;
	case OP_NEW: case OP_ANEWARRAY: case OP_CHECKCAST: case OP_INSTANCEOF: return 3;
	case OP_IFNULL: case OP_IFNONNULL: return 3;
	case OP_INVOKEINTERFACE: case OP_INVOKEDYNAMIC: case OP_GOTO_W: case OP_JSR_W: return 5;
	case OP_MULTIANEWARRAY: return 4;
	case OP_ARRAYLENGTH: case OP_ATHROW: case OP_MONITORENTER: case OP_MONITOREXIT: return 1;
	default: break;
	}
	if (op >= OP_ILOAD && op <= OP_ALOAD) return 2;
	if (op >= OP_ILOAD_0 && op <= OP_SALOAD) return 1;
	if (op >= OP_ISTORE && op <= OP_ASTORE) return 2;
	if (op >= OP_ISTORE_0 && op <= OP_LXOR) return 1;
	if (op >= OP_I2L && op <= OP_DCMPG) return 1;
	if (op >= OP_IFEQ && op <= OP_JSR) return 3;
	if (op >= OP_IRETURN && op <= OP_RETURN) return 1;
	if (op >= OP_GETSTATIC && op <= OP_INVOKESTATIC) return 3;
	return 0;
}

static u32 rd_u2(const u8 *p) {
	return (u32)p[0] << 8 | p[1];
}

static i32 rd_s4(const u8 *p) {
	return (i32)((u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]);
}

/* Length of the instruction at pc, 0 if it is invalid or runs past the end. */
static u32 insn_length(const u8 *code, u32 len, u32 pc) {
	u8 op = code[pc];
	u32 n;
	if (op == OP_TABLESWITCH || op == OP_LOOKUPSWITCH) {
		u32 pad = (4 - (pc + 1) % 4) % 4;
		u32 head = 1 + pad + (op == OP_TABLESWITCH ? 12 : 8);
		if ((u64)pc + head > len) return 0;
		const u8 *p = code + pc + 1 + pad;
		u64 count;
		if (op == OP_TABLESWITCH) {
			i32 low = rd_s4(p + 4), high = rd_s4(p + 8);
			if (high < low) return 0;
			count = (u64)((i64)high - low + 1) * 4;
		} else {
			i32 pairs = rd_s4(p + 4);
			if (pairs < 0) return 0;
			count = (u64)pairs * 8;
		}
		if ((u64)pc + head + count > len) return 0;
		return head + (u32)count;
	}
	if (op == OP_WIDE) {
		if (pc + 1 >= len) return 0;
		u8 op2 = code[pc + 1];
		if (op2 == OP_IINC) n = 6;
		else if ((op2 >= OP_ILOAD && op2 <= OP_ALOAD) || (op2 >= OP_ISTORE && op2 <= OP_ASTORE) || op2 == OP_RET) n = 4;
		else return 0;
	} else {
		n = fixed_len(op);
		if (n == 0) return 0;
	}
	return (u64)pc + n <= len ? n : 0;
}

typedef struct Check {
	VM *vm;
	Method *m;
	const ClassFile *cf;
	const u8 *code;
	u32 len;
	u8 *starts;   /* 1 at the first byte of every instruction */
	const char *why;
} Check;

static int target_ok(Check *k, i64 target) {
	return target >= 0 && target < k->len && k->starts[target];
}

static int local_ok(Check *k, u32 index, int wide) {
	return index + (wide ? 1u : 0u) < k->m->code->max_locals;
}

static int cp_ok(Check *k, u32 index, u8 tag1, u8 tag2) {
	if (index == 0 || index >= k->cf->cp_count) return 0;
	u8 t = k->cf->cp[index].tag;
	return t == tag1 || t == tag2;
}

static int check_insn(Check *k, u32 pc) {
	const u8 *c = k->code + pc;
	u8 op = c[0];
	if (op == OP_WIDE) {
		u8 op2 = c[1];
		u32 index = rd_u2(c + 2);
		int wide = op2 == OP_LLOAD || op2 == OP_DLOAD || op2 == OP_LSTORE || op2 == OP_DSTORE;
		return local_ok(k, index, wide);
	}
	if (op >= OP_ILOAD && op <= OP_ALOAD) return local_ok(k, c[1], op == OP_LLOAD || op == OP_DLOAD);
	if (op >= OP_ISTORE && op <= OP_ASTORE) return local_ok(k, c[1], op == OP_LSTORE || op == OP_DSTORE);
	if (op >= OP_ILOAD_0 && op <= OP_ALOAD_3) {
		u32 group = (op - OP_ILOAD_0) / 4;
		return local_ok(k, (op - OP_ILOAD_0) % 4, group == 1 || group == 3);
	}
	if (op >= OP_ISTORE_0 && op <= OP_ASTORE_3) {
		u32 group = (op - OP_ISTORE_0) / 4;
		return local_ok(k, (op - OP_ISTORE_0) % 4, group == 1 || group == 3);
	}
	switch (op) {
	case OP_IINC:
	case OP_RET:
		return local_ok(k, c[1], 0);
	case OP_LDC:
		return cp_ok(k, c[1], CP_INTEGER, CP_FLOAT) || cp_ok(k, c[1], CP_STRING, CP_CLASS);
	case OP_LDC_W:
		return cp_ok(k, rd_u2(c + 1), CP_INTEGER, CP_FLOAT) || cp_ok(k, rd_u2(c + 1), CP_STRING, CP_CLASS);
	case OP_LDC2_W:
		return cp_ok(k, rd_u2(c + 1), CP_LONG, CP_DOUBLE);
	case OP_GETSTATIC: case OP_PUTSTATIC: case OP_GETFIELD: case OP_PUTFIELD:
		return cp_ok(k, rd_u2(c + 1), CP_FIELDREF, CP_FIELDREF);
	case OP_INVOKEVIRTUAL:
		return cp_ok(k, rd_u2(c + 1), CP_METHODREF, CP_METHODREF);
	case OP_INVOKESPECIAL: case OP_INVOKESTATIC:
		return cp_ok(k, rd_u2(c + 1), CP_METHODREF, CP_INTERFACE_METHODREF);
	case OP_INVOKEINTERFACE:
		return cp_ok(k, rd_u2(c + 1), CP_INTERFACE_METHODREF, CP_INTERFACE_METHODREF) && c[3] != 0;
	case OP_INVOKEDYNAMIC:
		k->why = "invokedynamic is not supported";
		return 0;
	case OP_NEW: case OP_ANEWARRAY: case OP_CHECKCAST: case OP_INSTANCEOF:
		return cp_ok(k, rd_u2(c + 1), CP_CLASS, CP_CLASS);
	case OP_MULTIANEWARRAY:
		return cp_ok(k, rd_u2(c + 1), CP_CLASS, CP_CLASS) && c[3] >= 1;
	case OP_NEWARRAY:
		return c[1] >= T_BOOLEAN && c[1] <= T_LONG;
	default:
		break;
	}
	if ((op >= OP_IFEQ && op <= OP_JSR) || op == OP_IFNULL || op == OP_IFNONNULL)
		return target_ok(k, (i64)pc + (i16)rd_u2(c + 1));
	if (op == OP_GOTO_W || op == OP_JSR_W) return target_ok(k, (i64)pc + rd_s4(c + 1));
	if (op == OP_TABLESWITCH || op == OP_LOOKUPSWITCH) {
		const u8 *p = c + 1 + (4 - (pc + 1) % 4) % 4;
		if (!target_ok(k, (i64)pc + rd_s4(p))) return 0;
		if (op == OP_TABLESWITCH) {
			i64 n = (i64)rd_s4(p + 8) - rd_s4(p + 4) + 1;
			for (i64 i = 0; i < n; i++)
				if (!target_ok(k, (i64)pc + rd_s4(p + 12 + 4 * i))) return 0;
		} else {
			i32 n = rd_s4(p + 4);
			for (i32 i = 0; i < n; i++)
				if (!target_ok(k, (i64)pc + rd_s4(p + 12 + 8 * i))) return 0;
		}
	}
	return 1;
}

static int check(Check *k) {
	const CodeAttr *code = k->m->code;
	if (k->m->arg_slots > code->max_locals) {
		k->why = "arguments do not fit in max_locals";
		return 0;
	}
	u32 last = 0;
	for (u32 pc = 0; pc < k->len;) {
		u32 n = insn_length(k->code, k->len, pc);
		if (n == 0) {
			k->why = "bad instruction";
			return 0;
		}
		k->starts[pc] = 1;
		last = pc;
		pc += n;
	}
	/* Execution must not run off the end of the code. */
	u8 op = k->code[last];
	if (!(op == OP_GOTO || op == OP_GOTO_W || op == OP_ATHROW || op == OP_RET || op == OP_TABLESWITCH
			|| op == OP_LOOKUPSWITCH || (op >= OP_IRETURN && op <= OP_RETURN))) {
		k->why = "code falls off its end";
		return 0;
	}
	for (u32 pc = 0; pc < k->len; pc++) {
		if (!k->starts[pc]) continue;
		if (!check_insn(k, pc)) {
			if (k->why == NULL) k->why = "bad operand";
			return 0;
		}
	}
	for (u32 i = 0; i < code->handler_count; i++) {
		const ExceptionEntry *h = &code->handlers[i];
		if (!k->starts[h->start_pc] || !k->starts[h->handler_pc] || (h->end_pc < k->len && !k->starts[h->end_pc])) {
			k->why = "bad exception handler";
			return 0;
		}
	}
	return 1;
}

int method_check_pc(VM *vm, Method *m, u32 pc) {
	if (m->code == NULL || pc >= m->code->code_len) return 0;
	Check k = {vm, m, m->owner->cf, m->code->code, m->code->code_len, NULL, NULL};
	k.starts = calloc(k.len, 1);
	if (k.starts == NULL) return 0;
	int ok = check(&k) && k.starts[pc];
	free(k.starts);
	if (ok) m->prepared = 1;
	return ok;
}

int method_prepare(VM *vm, Method *m) {
	if (m->prepared) return 1;
	if (m->code == NULL) {
		throw_new(vm, "java/lang/AbstractMethodError", m->name);
		return 0;
	}
	Check k = {vm, m, m->owner->cf, m->code->code, m->code->code_len, NULL, NULL};
	k.starts = calloc(k.len, 1);
	if (k.starts == NULL) {
		vm->current->exception = vm->oom_error;
		return 0;
	}
	int ok = check(&k);
	free(k.starts);
	if (!ok) {
		char msg[256];
		snprintf(msg, sizeof msg, "%s.%s%s: %s", m->owner->name, m->name, m->desc, k.why);
		throw_new(vm, "java/lang/VerifyError", msg);
		return 0;
	}
	m->prepared = 1;
	return 1;
}
