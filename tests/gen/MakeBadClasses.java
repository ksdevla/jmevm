import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.FileOutputStream;
import java.io.IOException;

/**
 * Writes Bad1.class ... Bad6.class, each a public class whose public no-arg
 * constructor has malformed code that the VM must reject with VerifyError
 * instead of running it:
 *
 *   Bad1  branch past the end of the code
 *   Bad2  code that falls off its end
 *   Bad3  local variable index beyond max_locals
 *   Bad4  undefined opcode
 *   Bad5  ldc of a constant pool entry that is not a constant
 *   Bad6  branch into the middle of an instruction
 *
 * Usage: java MakeBadClasses.java <output directory>
 */
public class MakeBadClasses {
	public static void main(String[] args) throws IOException {
		int[][] bodies = {
			{0xA7, 0x00, 0x64, 0xB1},          // goto +100; return
			{0x00},                            // nop (and nothing after it)
			{0x15, 0x05, 0x57, 0xB1},          // iload 5; pop; return
			{0xCB, 0xB1},                      // undefined opcode; return
			{0x12, 0x09, 0x57, 0xB1},          // ldc #9 (a Utf8); pop; return
			{0xA7, 0xFF, 0xFF, 0xB1},          // goto -1 (into invokespecial); return
		};
		for (int i = 0; i < bodies.length; i++) {
			String name = "Bad" + (i + 1);
			try (FileOutputStream f = new FileOutputStream(args[0] + "/" + name + ".class")) {
				f.write(make(name, bodies[i]));
			}
		}
	}

	static byte[] make(String name, int[] body) throws IOException {
		ByteArrayOutputStream bytes = new ByteArrayOutputStream();
		DataOutputStream out = new DataOutputStream(bytes);
		out.writeInt(0xCAFEBABE);
		out.writeShort(0);
		out.writeShort(49);
		out.writeShort(10);                 // constant pool count (entries 1..9)
		utf8(out, name);                    // 1
		out.writeByte(7); out.writeShort(1);  // 2 Class
		utf8(out, "java/lang/Object");      // 3
		out.writeByte(7); out.writeShort(3);  // 4 Class
		utf8(out, "<init>");                // 5
		utf8(out, "()V");                   // 6
		out.writeByte(12); out.writeShort(5); out.writeShort(6);  // 7 NameAndType
		out.writeByte(10); out.writeShort(4); out.writeShort(7);  // 8 Methodref Object.<init>
		utf8(out, "Code");                  // 9

		out.writeShort(0x21);               // public super
		out.writeShort(2);
		out.writeShort(4);
		out.writeShort(0);                  // interfaces
		out.writeShort(0);                  // fields
		out.writeShort(1);                  // methods
		out.writeShort(0x01);               // public <init>()V
		out.writeShort(5);
		out.writeShort(6);
		out.writeShort(1);
		// aload_0; invokespecial Object.<init>; then the bad body
		int[] code = new int[4 + body.length];
		code[0] = 0x2A;
		code[1] = 0xB7;
		code[2] = 0x00;
		code[3] = 0x08;
		System.arraycopy(body, 0, code, 4, body.length);
		out.writeShort(9);
		out.writeInt(2 + 2 + 4 + code.length + 2 + 2);
		out.writeShort(2);                  // max_stack
		out.writeShort(1);                  // max_locals
		out.writeInt(code.length);
		for (int b : code) out.writeByte(b);
		out.writeShort(0);                  // exception table
		out.writeShort(0);                  // code attributes
		out.writeShort(0);                  // class attributes
		return bytes.toByteArray();
	}

	static void utf8(DataOutputStream out, String s) throws IOException {
		out.writeByte(1);
		out.writeUTF(s);
	}
}
