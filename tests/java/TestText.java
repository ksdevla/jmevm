import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.EOFException;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.PrintStream;
import java.io.Reader;
import java.io.UnsupportedEncodingException;

/*
 * Strings, string buffers, character encodings and the java.io streams.
 * The expected output is what a desktop JVM prints.
 */
public class TestText {
    public static void main(String[] args) throws Exception {
        strings();
        searching();
        buffers();
        encodings();
        readers();
        data();
        streams();
        System.out.println("done");
    }

    static void strings() {
        String s = "  Hello, World  ";
        String t = s.trim();
        System.out.println("[" + t + "] " + t.length() + " " + t.charAt(7) + " " + t.toUpperCase() + " " + t.toLowerCase());
        System.out.println(t.substring(7) + " " + t.substring(0, 5) + " " + t.substring(5, 5).length() + " "
                + t.concat("!") + " " + t.replace('l', 'L') + " " + "".trim().length() + " " + "\t\n x \r".trim());
        System.out.println("equal " + t.equals("Hello, World") + " " + t.equalsIgnoreCase("HELLO, world") + " "
                + t.equals(null) + " " + "é".equalsIgnoreCase("É") + " " + (t.substring(0) == t));
        System.out.println("compare " + "apple".compareTo("banana") + " " + "b".compareTo("a") + " "
                + "abc".compareTo("abcde") + " " + "same".compareTo("same"));
        System.out.println("hash " + "".hashCode() + " " + "a".hashCode() + " " + "Hello, World".hashCode() + " "
                + "аб".hashCode());
        System.out.println("valueOf " + String.valueOf(true) + " " + String.valueOf('c') + " " + String.valueOf(-12)
                + " " + String.valueOf(1L << 40) + " " + String.valueOf(0.5f) + " " + String.valueOf(1e100) + " "
                + String.valueOf((Object) null) + " " + String.valueOf(new char[] {'o', 'k'}) + " "
                + String.valueOf(new char[] {'a', 'b', 'c', 'd'}, 1, 2));
        char[] dst = new char[5];
        "abcdef".getChars(1, 4, dst, 1);
        System.out.println("getChars " + new String(dst, 1, 3) + " " + new String("abc".toCharArray()));
        String interned = new String("lit").intern();
        System.out.println("intern " + (interned == "lit") + " " + (new String("lit") == "lit"));
        try {
            "abc".charAt(3);
        } catch (StringIndexOutOfBoundsException e) {
            System.out.println("charAt out of range");
        }
        try {
            "abc".substring(2, 1);
        } catch (StringIndexOutOfBoundsException e) {
            System.out.println("substring out of range");
        }
    }

    static void searching() {
        String s = "the cat sat on the mat";
        System.out.println("indexOf " + s.indexOf('a') + " " + s.indexOf('a', 6) + " " + s.indexOf('z') + " "
                + s.indexOf("the") + " " + s.indexOf("the", 1) + " " + s.indexOf("") + " " + s.indexOf("", 99) + " "
                + s.indexOf('t', -5));
        System.out.println("lastIndexOf " + s.lastIndexOf('a') + " " + s.lastIndexOf('a', 10) + " "
                + s.lastIndexOf("the") + " " + s.lastIndexOf("the", 14) + " " + s.lastIndexOf("") + " "
                + s.lastIndexOf('t', -1) + " " + s.lastIndexOf("at", 100));
        System.out.println("starts " + s.startsWith("the") + " " + s.startsWith("cat", 4) + " "
                + s.startsWith("cat", -1) + " " + s.endsWith("mat") + " " + s.endsWith("") + " "
                + s.regionMatches(true, 4, "CAT", 0, 3) + " " + s.regionMatches(4, "CAT", 0, 3) + " "
                + s.regionMatches(0, "x", 0, -1));
    }

    static void buffers() {
        StringBuffer sb = new StringBuffer("start");
        sb.append(' ').append(42).append(' ').append(-7L).append(' ').append(1.25).append(' ').append(true)
                .append(' ').append('c').append(' ').append((Object) null).append(' ').append(new char[] {'x', 'y'});
        System.out.println(sb + " " + sb.length());
        sb.setLength(5);
        sb.insert(0, "re").insert(2, 1).insert(sb.length(), '!').insert(3, 2.5f).insert(0, false);
        System.out.println(sb + " " + sb.charAt(5));
        sb.delete(0, 5).deleteCharAt(0).reverse();
        sb.setCharAt(0, '?');
        System.out.println(sb + " " + sb.length());
        sb.delete(2, 1000);
        System.out.println(sb);
        StringBuffer grow = new StringBuffer(2);
        int before = grow.capacity();
        grow.append("0123456789");
        System.out.println("capacity " + before + " " + (grow.capacity() >= 10) + " " + grow);
        grow.setLength(12);
        System.out.println("padded " + grow.length() + " " + (int) grow.charAt(11));
        StringBuilder b = new StringBuilder();
        for (int i = 0; i < 5; i++) {
            b.append(i).append(',');
        }
        b.setLength(b.length() - 1);
        System.out.println("builder " + b + " " + b.reverse() + " " + b.length());
        try {
            sb.insert(99, "x");
        } catch (StringIndexOutOfBoundsException e) {
            System.out.println("insert out of range");
        }
        String surrogates = new StringBuffer("a😀b").reverse().toString();
        System.out.println("surrogates " + Integer.toHexString(surrogates.charAt(1)) + " "
                + Integer.toHexString(surrogates.charAt(2)));
        int n = 7;
        String concat = "n=" + n + ", half=" + n / 2.0 + ", char=" + (char) ('a' + n) + ", null=" + null;
        System.out.println(concat);
    }

    static void encodings() throws Exception {
        String text = "Aé€Ж😀";
        String[] names = {"UTF-8", "ISO-8859-1", "US-ASCII", "UTF-16BE", "UTF-16LE", "UTF-16", "windows-1251",
                "windows-1252"};
        for (int i = 0; i < names.length; i++) {
            byte[] bytes = text.getBytes(names[i]);
            String back = new String(bytes, names[i]);
            System.out.println(names[i] + " " + hex(bytes) + " -> " + codes(back));
        }
        System.out.println("default " + hex("éxЖ".getBytes()) + " " + codes(new String(new byte[] {
                (byte) 0xe9, 0x41})));
        byte[][] malformed = {{(byte) 0xc3}, {(byte) 0xe2, (byte) 0x82}, {(byte) 0xff, 0x41}, {(byte) 0xc0, (byte) 0x80},
                {(byte) 0xed, (byte) 0xa0, (byte) 0x80}, {(byte) 0xf0, (byte) 0x9f, 0x41}, {(byte) 0xe2, 0x41, 0x42}};
        for (int i = 0; i < malformed.length; i++) {
            System.out.println("malformed " + hex(malformed[i]) + " -> " + codes(new String(malformed[i], "UTF-8")));
        }
        System.out.println("bom " + codes(new String(new byte[] {(byte) 0xff, (byte) 0xfe, 0x41, 0}, "UTF-16")) + " "
                + codes(new String(new byte[] {(byte) 0xfe, (byte) 0xff}, "UTF-16")) + " "
                + codes(new String(new byte[] {0, 0x41, 0}, "UTF-16BE")));
        System.out.println("offsets " + new String("xxhixx".getBytes("UTF-8"), 2, 2, "UTF-8") + " "
                + new String("abc".getBytes(), 1, 2));
        try {
            "x".getBytes("no-such-encoding");
        } catch (UnsupportedEncodingException e) {
            System.out.println("unsupported encoding");
        }
    }

    static void readers() throws IOException {
        StringBuffer big = new StringBuffer();
        for (int i = 0; i < 300; i++) {
            big.append((char) ('a' + i % 26)).append(i % 50 == 0 ? "€😀" : "");
        }
        String text = big.toString();
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        OutputStreamWriter w = new OutputStreamWriter(bytes, "UTF-8");
        w.write(text.substring(0, 100));
        w.write(text, 100, 50);
        char[] rest = text.substring(150).toCharArray();
        for (int i = 0; i < rest.length; i++) {
            w.write(rest[i]);   /* surrogate pairs arrive a half at a time */
        }
        w.write(text.toCharArray(), 0, 0);
        w.flush();
        w.close();
        byte[] utf8 = bytes.toByteArray();
        System.out.println("written " + utf8.length + " " + (new String(utf8, "UTF-8").equals(text)));
        Reader r = new InputStreamReader(new ByteArrayInputStream(utf8), "UTF-8");
        StringBuffer read = new StringBuffer();
        char[] chunk = new char[7];
        int n;
        while ((n = r.read(chunk, 0, chunk.length)) > 0) {
            read.append(chunk, 0, n);
        }
        System.out.println("read back " + read.length() + " " + read.toString().equals(text) + " " + r.read());
        r.close();
        Reader one = new InputStreamReader(new ByteArrayInputStream(new byte[] {0x41, (byte) 0xc3, (byte) 0xa9,
                (byte) 0xe2}), "UTF-8");
        StringBuffer chars = new StringBuffer();
        int c;
        while ((c = one.read()) >= 0) {
            chars.append(Integer.toHexString(c)).append(' ');
        }
        System.out.println("chars " + chars);
        Reader latin = new InputStreamReader(new ByteArrayInputStream("skip me: yes".getBytes()));
        System.out.println("skip " + latin.skip(9) + " " + (char) latin.read() + " " + latin.markSupported());
        try {
            latin.reset();
        } catch (IOException e) {
            System.out.println("no reset");
        }
        latin.close();
        Reader closed = new InputStreamReader(new ByteArrayInputStream("abc".getBytes()));
        closed.close();
        try {
            closed.read();
        } catch (IOException e) {
            System.out.println("closed");
        }
    }

    static void data() throws IOException {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        DataOutputStream out = new DataOutputStream(bytes);
        out.writeBoolean(true);
        out.writeByte(-2);
        out.writeShort(-300);
        out.writeChar('€');
        out.writeInt(0xcafebabe);
        out.writeLong(-1234567890123456789L);
        out.writeFloat(3.5f);
        out.writeDouble(-0.1);
        out.writeUTF("hé\u0000€😀");
        out.writeChars("ok");
        out.write(new byte[] {9, 8, 7}, 1, 2);
        out.close();
        byte[] all = bytes.toByteArray();
        System.out.println("data " + all.length + " " + hex(all));
        DataInputStream in = new DataInputStream(new ByteArrayInputStream(all));
        System.out.println("read " + in.readBoolean() + " " + in.readByte() + " " + in.readShort() + " "
                + Integer.toHexString(in.readChar()) + " " + Integer.toHexString(in.readInt()) + " " + in.readLong() + " "
                + in.readFloat() + " " + in.readDouble());
        System.out.println("utf " + codes(in.readUTF()) + " " + in.readChar() + in.readChar() + " "
                + in.readUnsignedByte() + " " + in.available());
        in.mark(10);
        System.out.println("last " + in.readUnsignedByte() + " " + in.read());
        in.reset();
        System.out.println("reset " + in.read() + " " + in.skipBytes(5));
        try {
            in.readInt();
        } catch (EOFException e) {
            System.out.println("eof");
        }
        byte[] full = new byte[4];
        try {
            new DataInputStream(new ByteArrayInputStream(new byte[3])).readFully(full);
        } catch (EOFException e) {
            System.out.println("short read");
        }
        try {
            new DataInputStream(new ByteArrayInputStream(new byte[] {0, 2, (byte) 0xc3, 0x41})).readUTF();
        } catch (IOException e) {
            System.out.println("bad utf " + e.getClass().getName());
        }
        StringBuffer huge = new StringBuffer();
        for (int i = 0; i < 22000; i++) {
            huge.append('€');
        }
        try {
            new DataOutputStream(new ByteArrayOutputStream()).writeUTF(huge.toString());
        } catch (IOException e) {
            System.out.println("too long " + e.getClass().getName());
        }
    }

    static void streams() throws IOException {
        byte[] data = new byte[20];
        for (int i = 0; i < data.length; i++) {
            data[i] = (byte) (i * 13);
        }
        ByteArrayInputStream in = new ByteArrayInputStream(data, 5, 10);
        byte[] buf = new byte[4];
        int n = in.read(buf, 1, 3);
        System.out.println("window " + n + " " + hex(buf) + " " + in.available() + " " + in.skip(100) + " "
                + in.read() + " " + in.read(buf, 0, 0) + " " + in.read(buf, 0, 1));
        in.reset();
        System.out.println("reset to " + in.read());
        try {
            in.read(buf, 3, 2);
        } catch (IndexOutOfBoundsException e) {
            System.out.println("bad range");
        }
        ByteArrayOutputStream sink = new ByteArrayOutputStream(1);
        PrintStream p = new PrintStream(sink);
        p.print(1);
        p.print('-');
        p.print(2L);
        p.print(' ');
        p.print(0.25f);
        p.print(' ');
        p.print(1e-5);
        p.print(' ');
        p.print(false);
        p.print(' ');
        p.print((String) null);
        p.print(new char[] {' ', '#'});
        p.println();
        p.println("line");
        p.println((Object) "obj");
        p.println(new char[] {'c', 'h'});
        p.println(7);
        p.println('z');
        p.println(true);
        p.println(3L);
        p.println(1.5f);
        p.println(2.5);
        p.write(0x41);
        p.write(new byte[] {0x42, 0x43}, 0, 2);
        p.flush();
        System.out.println("printed " + sink.size() + " [" + sink.toString().replace('\n', '|') + "] " + p.checkError());
        p.close();
        p.print("after close");
        System.out.println("error after close " + p.checkError());
        sink.reset();
        sink.write(new byte[] {0x61, 0x62}, 0, 2);
        System.out.println("reset sink " + sink + " " + sink.toByteArray().length);
    }

    static String hex(byte[] b) {
        StringBuffer sb = new StringBuffer();
        for (int i = 0; i < b.length; i++) {
            sb.append(Integer.toHexString((b[i] & 0xff) | 0x100).substring(1));
        }
        return sb.toString();
    }

    static String codes(String s) {
        StringBuffer sb = new StringBuffer();
        for (int i = 0; i < s.length(); i++) {
            if (i > 0) {
                sb.append(' ');
            }
            sb.append(Integer.toHexString(s.charAt(i)));
        }
        return sb.toString();
    }
}
