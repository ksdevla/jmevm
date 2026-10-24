/*
 * The language and java.lang: arithmetic, conversions, the wrapper classes,
 * Math, exceptions, classes, arrays and initialization. The expected output
 * is what a desktop JVM prints.
 */
public class TestLang {
    interface Shape {
        int area();
    }

    static class Rect implements Shape {
        final int w;
        final int h;

        Rect(int w, int h) {
            this.w = w;
            this.h = h;
        }

        public int area() {
            return w * h;
        }

        public String toString() {
            return "Rect " + w + "x" + h;
        }
    }

    static class Square extends Rect {
        Square(int side) {
            super(side, side);
        }

        public String toString() {
            return "Square " + w;
        }
    }

    static class GameOver extends Exception {
        final int score;

        GameOver(String message, int score) {
            super(message);
            this.score = score;
        }
    }

    static String order = "";

    static class First {
        static final int A = mark("First");
    }

    static class Second extends First {
        static final int B = mark("Second");

        static void touch() {
        }
    }

    static int mark(String s) {
        order += s + " ";
        return order.length();
    }

    public static void main(String[] args) {
        integers();
        longs();
        floats();
        wrappers();
        characters();
        math();
        exceptions();
        classes();
        arrays();
        control();
        System.out.println("done");
    }

    static void integers() {
        int max = Integer.MAX_VALUE;
        int min = Integer.MIN_VALUE;
        System.out.println("overflow " + (max + 1) + " " + (min - 1) + " " + (-min) + " " + (min / -1));
        System.out.println("div " + (7 / 2) + " " + (-7 / 2) + " " + (7 % -2) + " " + (-7 % 2));
        System.out.println("shift " + (1 << 33) + " " + (-16 >> 2) + " " + (-16 >>> 28) + " " + (5 << -1));
        System.out.println("narrow " + (byte) 200 + " " + (short) 70000 + " " + (char) 65 + " " + (int) 'z');
        int x = 0x12345678;
        System.out.println("bits " + (x & 0xff) + " " + (x | 1) + " " + (x ^ -1) + " " + ~x);
        try {
            System.out.println(1 / (x - x));
        } catch (ArithmeticException e) {
            System.out.println("divide by zero caught");
        }
        int i = 5;
        i += i++ + ++i;
        System.out.println("increments " + i);
    }

    static void longs() {
        long a = 0x7fffffffffffffffL;
        System.out.println("long " + (a + 1) + " " + (a * 3) + " " + (a >>> 60) + " " + (-a >> 70));
        long b = 1234567890123L;
        System.out.println("long ops " + b / 1000 + " " + b % 1000 + " " + (b * -b) + " " + (int) b);
        System.out.println("compare " + (b > 0 ? "pos" : "neg") + " " + Long.MIN_VALUE + " " + Long.MAX_VALUE);
    }

    static void floats() {
        double d = 1.0 / 3;
        float f = 1.0f / 3;
        System.out.println("double " + d + " " + (d * 3) + " " + (0.1 + 0.2) + " " + 1e21 + " " + 1e-7);
        System.out.println("float " + f + " " + (f * 3) + " " + 100.0f + " " + 1.5e10f + " " + 3.4e38f);
        System.out.println("special " + (1 / 0.0) + " " + (-1 / 0.0) + " " + (0.0 / 0.0) + " " + -0.0);
        System.out.println("to int " + (int) 3.99 + " " + (int) -3.99 + " " + (int) 1e20 + " " + (int) Double.NaN
                + " " + (long) -1e30 + " " + (int) Float.NEGATIVE_INFINITY);
        System.out.println("compare " + (Double.NaN == Double.NaN) + " " + (0.0 == -0.0) + " " + (Double.NaN < 1)
                + " " + Double.isNaN(0.0 / 0.0) + " " + Float.isInfinite(1 / 0.0f));
        System.out.println("bits " + Long.toString(Double.doubleToLongBits(1.5), 16) + " "
                + Integer.toHexString(Float.floatToIntBits(-2.25f)) + " " + Double.longBitsToDouble(0x4010000000000000L)
                + " " + Float.intBitsToFloat(0x3f000000));
        System.out.println("parse " + Double.parseDouble(" 12.5e3 ") + " " + Float.parseFloat("-0.001") + " "
                + Double.valueOf("1e-300").doubleValue() + " " + Double.parseDouble("Infinity"));
        System.out.println("remainder " + (7.5 % 2) + " " + (-7.5 % 2) + " " + (5.0f % 0.0f));
        System.out.println("min " + Double.MIN_VALUE + " " + Float.MIN_VALUE + " max " + Double.MAX_VALUE + " "
                + Float.MAX_VALUE);
        Double boxed = new Double(2.5);
        System.out.println("boxed " + boxed + " " + boxed.intValue() + " " + boxed.equals(new Double(2.5)) + " "
                + new Double(0.0).equals(new Double(-0.0)) + " " + new Double(Double.NaN).equals(new Double(Double.NaN))
                + " " + boxed.hashCode() + " " + new Float(2.5f).hashCode());
    }

    static void wrappers() {
        System.out.println("toString " + Integer.toString(255, 16) + " " + Integer.toString(-255, 2) + " "
                + Integer.toString(35, 36) + " " + Integer.toString(7, 99) + " " + Long.toString(Long.MIN_VALUE, 7));
        System.out.println("unsigned " + Integer.toHexString(-1) + " " + Integer.toOctalString(-8) + " "
                + Integer.toBinaryString(10) + " " + Integer.toHexString(0));
        System.out.println("parse " + Integer.parseInt("-2147483648") + " " + Integer.parseInt("+77") + " "
                + Integer.parseInt("zz", 36) + " " + Long.parseLong("-9223372036854775808") + " "
                + Short.parseShort("-32768") + " " + Byte.parseByte("7f", 16));
        String[] bad = {"", "-", "2147483648", "12a", " 1", "99999999999999999999"};
        for (int i = 0; i < bad.length; i++) {
            try {
                System.out.println("parsed " + Integer.parseInt(bad[i]));
            } catch (NumberFormatException e) {
                System.out.println("bad number [" + bad[i] + "]");
            }
        }
        try {
            Byte.parseByte("128");
        } catch (NumberFormatException e) {
            System.out.println("byte range");
        }
        Integer seven = Integer.valueOf("7");
        System.out.println("boxes " + seven + " " + seven.equals(new Integer(7)) + " " + seven.equals(new Long(7))
                + " " + new Long(-1).hashCode() + " " + new Long(1L << 32).hashCode() + " " + Boolean.TRUE + " "
                + new Boolean(false).hashCode() + " " + new Character('x') + " " + new Short((short) -3).shortValue()
                + " " + new Byte((byte) 5).byteValue());
    }

    static void characters() {
        String s = "aZ5 éÉßÿаЯαΩ";
        StringBuffer up = new StringBuffer();
        StringBuffer low = new StringBuffer();
        StringBuffer kind = new StringBuffer();
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            up.append(Integer.toHexString(Character.toUpperCase(c))).append(' ');
            low.append(Integer.toHexString(Character.toLowerCase(c))).append(' ');
            kind.append(Character.isUpperCase(c) ? 'U' : Character.isLowerCase(c) ? 'l' : Character.isDigit(c) ? 'd' : '.');
        }
        System.out.println("upper " + up);
        System.out.println("lower " + low);
        System.out.println("kinds " + kind);
        System.out.println("digits " + Character.digit('7', 10) + " " + Character.digit('f', 16) + " "
                + Character.digit('F', 16) + " " + Character.digit('g', 16) + " " + Character.digit('z', 36) + " "
                + Character.digit('5', 2));
    }

    static void math() {
        System.out.println("abs " + Math.abs(-5) + " " + Math.abs(Integer.MIN_VALUE) + " " + Math.abs(-2.5) + " "
                + Math.abs(-0.0) + " " + Math.abs(-7L) + " " + Math.abs(-1.5f));
        System.out.println("minmax " + Math.min(3, -4) + " " + Math.max(3L, 9L) + " " + Math.min(-0.0, 0.0) + " "
                + Math.max(-0.0f, 0.0f) + " " + Math.max(Double.NaN, 1) + " " + Math.min(1.5f, 2.5f));
        System.out.println("round " + Math.floor(-1.5) + " " + Math.ceil(-1.5) + " " + Math.floor(2.0) + " "
                + Math.ceil(1e-300) + " " + Math.ceil(-0.5));
        System.out.println("roots " + Math.sqrt(2) + " " + Math.sqrt(-1) + " " + Math.sqrt(1e10));
        /* rounded: a desktop JVM's Math may differ from StrictMath in the last bit */
        System.out.println("trig " + round(Math.sin(Math.PI / 6)) + " " + round(Math.cos(1)) + " "
                + round(Math.tan(0.5)) + " " + round(Math.sin(1e10)));
        System.out.println("angles " + Math.toRadians(180) + " " + Math.toDegrees(1) + " " + Math.E);
    }

    static long round(double v) {
        return (long) Math.floor(v * 1e12 + 0.5);
    }

    static int depth(int n) throws GameOver {
        if (n == 0) {
            throw new GameOver("out of lives", 4200);
        }
        try {
            return depth(n - 1);
        } finally {
            order += n;
        }
    }

    static void exceptions() {
        order = "";
        try {
            depth(3);
        } catch (GameOver e) {
            System.out.println("caught " + e.getMessage() + " " + e.score + " finally order " + order);
            System.out.println("as string " + e.toString().substring(e.toString().indexOf('$') + 1));
        }
        Object[] tests = {null, "x", new int[1], new Object()};
        for (int i = 0; i < tests.length; i++) {
            try {
                switch (i) {
                    case 0:
                        tests[i].hashCode();
                        break;
                    case 1:
                        Integer bad = (Integer) tests[i];
                        System.out.println(bad);
                        break;
                    case 2:
                        ((int[]) tests[i])[1] = 1;
                        break;
                    default:
                        Object[] strings = new String[1];
                        strings[0] = tests[i];
                        break;
                }
            } catch (RuntimeException e) {
                System.out.println("runtime " + e.getClass().getName());
            }
        }
        try {
            int[] negative = new int[-1];
            System.out.println(negative.length);
        } catch (NegativeArraySizeException e) {
            System.out.println("runtime " + e.getClass().getName());
        }
        try {
            throw new Error("custom error");
        } catch (Throwable t) {
            System.out.println("throwable " + t);
        }
        System.out.println("plain " + new RuntimeException() + " " + new IllegalStateException("state").getMessage());
        System.out.println("finally returns " + finallyWins());
    }

    static int finallyWins() {
        int x = 1;
        try {
            return x;
        } finally {
            x = 2;
        }
    }

    static void classes() {
        Shape[] shapes = {new Rect(2, 3), new Square(4)};
        int total = 0;
        for (int i = 0; i < shapes.length; i++) {
            total += shapes[i].area();
            System.out.println(shapes[i] + " is rect " + (shapes[i] instanceof Rect) + " square "
                    + (shapes[i] instanceof Square));
        }
        System.out.println("total area " + total);
        System.out.println("names " + "s".getClass().getName() + " " + new int[0].getClass().getName() + " "
                + new String[0][0].getClass().getName() + " " + Shape.class.getName());
        try {
            Class c = Class.forName("java.util.Vector");
            Object v = c.newInstance();
            System.out.println("forName " + c.getName() + " " + c.isInstance(v) + " " + c.isInterface() + " "
                    + Class.forName("java.lang.Runnable").isInterface() + " " + c.isArray() + " "
                    + Object.class.isAssignableFrom(c) + " " + c.isAssignableFrom(Object.class));
            Class.forName("no.such.Thing");
        } catch (ClassNotFoundException e) {
            System.out.println("not found");
        } catch (Exception e) {
            System.out.println("unexpected " + e);
        }
        order = "";
        Second.touch();
        System.out.println("init order " + order + Second.B);
        Object o = new Object();
        System.out.println("identity " + (o.hashCode() == System.identityHashCode(o)) + " " + o.equals(o) + " "
                + o.equals(new Object()) + " " + (o.toString().startsWith("java.lang.Object@")));
    }

    static void arrays() {
        int[][] grid = new int[3][4];
        for (int y = 0; y < 3; y++) {
            for (int x = 0; x < 4; x++) {
                grid[y][x] = y * 10 + x;
            }
        }
        int[] row = (int[]) grid[2].clone();
        row[0] = 99;
        System.out.println("grid " + grid[2][0] + " " + row[0] + " " + grid.length + " " + grid[1].length);
        int[][] ragged = {{1}, {2, 3}, {}};
        System.out.println("ragged " + ragged[1][1] + " " + ragged[2].length);
        char[] chars = {'h', 'e', 'l', 'l', 'o'};
        System.arraycopy(chars, 1, chars, 0, 4);
        System.out.println("arraycopy " + new String(chars));
        long[] longs = new long[3];
        boolean[] flags = new boolean[2];
        double[] doubles = new double[1];
        Object[] objects = new Object[1];
        System.out.println("defaults " + longs[2] + " " + flags[1] + " " + doubles[0] + " " + objects[0]);
        try {
            System.arraycopy(chars, 0, longs, 0, 1);
        } catch (ArrayStoreException e) {
            System.out.println("array store");
        }
        try {
            System.arraycopy(chars, 3, chars, 0, 5);
        } catch (IndexOutOfBoundsException e) {
            System.out.println("copy out of bounds");
        }
    }

    static String name(int day) {
        switch (day) {
            case 1:
                return "mon";
            case 2:
                return "tue";
            case 3:
            case 4:
                return "mid";
            case 1000:
                return "far";
            default:
                return "other";
        }
    }

    static void control() {
        StringBuffer sb = new StringBuffer();
        int[] days = {1, 2, 3, 4, 5, 1000, -1};
        for (int i = 0; i < days.length; i++) {
            sb.append(name(days[i])).append(' ');
        }
        System.out.println("switch " + sb);
        int found = -1;
        outer:
        for (int i = 0; i < 10; i++) {
            for (int j = 0; j < 10; j++) {
                if (i * j == 42) {
                    found = i * 10 + j;
                    break outer;
                }
                if (j > i) {
                    continue outer;
                }
            }
        }
        System.out.println("labels " + found);
        int n = 0;
        do {
            n += 3;
        } while (n < 10);
        boolean lazy = n > 100 && 1 / (n - n) > 0;
        System.out.println("loops " + n + " " + lazy + " " + (n > 5 ? "big" : "small"));
    }
}
