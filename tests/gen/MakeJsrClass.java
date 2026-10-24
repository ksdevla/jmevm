import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.FileOutputStream;
import java.io.IOException;

/**
 * Writes JsrTest.class: class version 45.3 (as produced by javac 1.1-1.4 for
 * J2ME games) using jsr/ret, which modern javac can no longer emit.
 *
 *   main: a subroutine called twice                         -> prints 10
 *   g(int mode): try { 1 / (mode - 1); r = 1; } finally { r += 10; print r }
 *     compiled the old way: the finally block is a subroutine reached by
 *     jsr from the normal path and from a catch-all handler that rethrows.
 *     main prints g(0), then calls g(1) and catches the ArithmeticException.
 *
 * Expected output (JsrTest.expected): 10 11 11 10 -1, one per line.
 *
 * Usage: java MakeJsrClass.java <output file>
 */
public class MakeJsrClass {
	static final int JSR = 0xA8, RET = 0xA9, GOTO = 0xA7;

	public static void main(String[] args) throws IOException {
		ByteArrayOutputStream bytes = new ByteArrayOutputStream();
		DataOutputStream out = new DataOutputStream(bytes);

		out.writeInt(0xCAFEBABE);
		out.writeShort(3);    // minor
		out.writeShort(45);   // major
		out.writeShort(26);   // constant pool count (entries 1..25)
		utf8(out, "JsrTest");                     // 1
		ref(out, 7, 1);                           // 2 Class JsrTest
		utf8(out, "java/lang/Object");            // 3
		ref(out, 7, 3);                           // 4 Class Object
		utf8(out, "main");                        // 5
		utf8(out, "([Ljava/lang/String;)V");      // 6
		utf8(out, "Code");                        // 7
		utf8(out, "java/lang/System");            // 8
		ref(out, 7, 8);                           // 9
		utf8(out, "out");                         // 10
		utf8(out, "Ljava/io/PrintStream;");       // 11
		ref2(out, 12, 10, 11);                    // 12 NameAndType
		ref2(out, 9, 9, 12);                      // 13 Fieldref System.out
		utf8(out, "java/io/PrintStream");         // 14
		ref(out, 7, 14);                          // 15
		utf8(out, "println");                     // 16
		utf8(out, "(I)V");                        // 17
		ref2(out, 12, 16, 17);                    // 18
		ref2(out, 10, 15, 18);                    // 19 Methodref println(I)V
		utf8(out, "g");                           // 20
		utf8(out, "(I)I");                        // 21
		ref2(out, 12, 20, 21);                    // 22
		ref2(out, 10, 2, 22);                     // 23 Methodref JsrTest.g(I)I
		utf8(out, "java/lang/ArithmeticException"); // 24
		ref(out, 7, 24);                          // 25

		int[] main = {
			0x03,                   // 0  iconst_0
			0x3C,                   // 1  istore_1
			JSR, 0x00, 40,          // 2  jsr 42
			JSR, 0x00, 37,          // 5  jsr 42
			0xB2, 0x00, 13,         // 8  getstatic System.out
			0x1B,                   // 11 iload_1
			0xB6, 0x00, 19,         // 12 invokevirtual println(I)V      -> 10
			0xB2, 0x00, 13,         // 15 getstatic System.out
			0x03,                   // 18 iconst_0
			0xB8, 0x00, 23,         // 19 invokestatic g(I)I             -> 11 (finally)
			0xB6, 0x00, 19,         // 22 invokevirtual println(I)V      -> 11
			0x04,                   // 25 iconst_1   (try start)
			0xB8, 0x00, 23,         // 26 invokestatic g(I)I             -> 10 (finally), throws
			0x57,                   // 29 pop
			GOTO, 0x00, 11,         // 30 goto 41    (try end)
			0x4E,                   // 33 astore_3   (catch ArithmeticException)
			0xB2, 0x00, 13,         // 34 getstatic System.out
			0x02,                   // 37 iconst_m1
			0xB6, 0x00, 19,         // 38 invokevirtual println(I)V      -> -1
			0xB1,                   // 41 return
			0x4D,                   // 42 astore_2   (subroutine)
			0x84, 0x01, 0x05,       // 43 iinc 1, 5
			RET, 0x02,              // 46 ret 2
		};
		int[][] mainExc = { { 25, 30, 33, 25 } };

		int[] g = {
			0x03,                   // 0  iconst_0
			0x3C,                   // 1  istore_1   r = 0
			0x04,                   // 2  iconst_1   (try start)
			0x1A,                   // 3  iload_0
			0x04,                   // 4  iconst_1
			0x64,                   // 5  isub
			0x6C,                   // 6  idiv       1 / (mode - 1)
			0x57,                   // 7  pop
			0x04,                   // 8  iconst_1
			0x3C,                   // 9  istore_1   r = 1
			JSR, 0x00, 11,          // 10 jsr 21     (try end)
			0x1B,                   // 13 iload_1
			0xAC,                   // 14 ireturn
			0x4D,                   // 15 astore_2   (catch any)
			JSR, 0x00, 5,           // 16 jsr 21
			0x2C,                   // 19 aload_2
			0xBF,                   // 20 athrow
			0x4E,                   // 21 astore_3   (finally subroutine)
			0x84, 0x01, 10,         // 22 iinc 1, 10
			0xB2, 0x00, 13,         // 25 getstatic System.out
			0x1B,                   // 28 iload_1
			0xB6, 0x00, 19,         // 29 invokevirtual println(I)V
			RET, 0x03,              // 32 ret 3
		};
		int[][] gExc = { { 2, 10, 15, 0 } };

		out.writeShort(0x0021);   // public super
		out.writeShort(2);        // this
		out.writeShort(4);        // super
		out.writeShort(0);        // interfaces
		out.writeShort(0);        // fields
		out.writeShort(2);        // methods
		method(out, 0x0009, 5, 6, 2, 4, main, mainExc);
		method(out, 0x0008, 20, 21, 3, 4, g, gExc);
		out.writeShort(0);        // class attributes

		try (FileOutputStream f = new FileOutputStream(args[0])) {
			f.write(bytes.toByteArray());
		}
	}

	static void method(DataOutputStream out, int access, int name, int desc, int maxStack,
			int maxLocals, int[] code, int[][] exc) throws IOException {
		out.writeShort(access);
		out.writeShort(name);
		out.writeShort(desc);
		out.writeShort(1);        // attributes: Code
		out.writeShort(7);
		out.writeInt(12 + code.length + 8 * exc.length);
		out.writeShort(maxStack);
		out.writeShort(maxLocals);
		out.writeInt(code.length);
		for (int b : code)
			out.writeByte(b);
		out.writeShort(exc.length);
		for (int[] e : exc)
			for (int v : e)
				out.writeShort(v);
		out.writeShort(0);        // code attributes
	}

	static void utf8(DataOutputStream out, String s) throws IOException {
		out.writeByte(1);
		out.writeUTF(s);
	}

	static void ref(DataOutputStream out, int tag, int index) throws IOException {
		out.writeByte(tag);
		out.writeShort(index);
	}

	static void ref2(DataOutputStream out, int tag, int a, int b) throws IOException {
		out.writeByte(tag);
		out.writeShort(a);
		out.writeShort(b);
	}
}
