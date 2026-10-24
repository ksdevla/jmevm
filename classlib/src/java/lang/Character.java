package java.lang;

/**
 * A char as an object, and the character classes CLDC asks for. Case and
 * digits go beyond ISO Latin-1, which is all CLDC requires: games show
 * Greek, Cyrillic and Latin Extended-A text too.
 */
public final class Character {
    public static final int MIN_RADIX = 2;
    public static final int MAX_RADIX = 36;
    public static final char MIN_VALUE = '\u0000';
    public static final char MAX_VALUE = '￿';

    /*
     * The case pairs: {first upper, last upper, distance to lower, step}. A
     * step of 1 is a run of upper case letters; 2 alternates upper and lower
     * (U+0100 A with macron, U+0101 its lower case, ...). Sorted by first.
     */
    private static final int[] CASES = {
        0x0041, 0x005a, 32, 1,
        0x00c0, 0x00d6, 32, 1,
        0x00d8, 0x00de, 32, 1,
        0x0100, 0x012e, 1, 2,
        0x0132, 0x0136, 1, 2,
        0x0139, 0x0147, 1, 2,
        0x014a, 0x0176, 1, 2,
        0x0178, 0x0178, -121, 1,
        0x0179, 0x017d, 1, 2,
        0x0386, 0x0386, 38, 1,
        0x0388, 0x038a, 37, 1,
        0x038c, 0x038c, 64, 1,
        0x038e, 0x038f, 63, 1,
        0x0391, 0x03a1, 32, 1,
        0x03a3, 0x03ab, 32, 1,
        0x0400, 0x040f, 80, 1,
        0x0410, 0x042f, 32, 1,
        0x0460, 0x0480, 1, 2,
        0x048a, 0x04be, 1, 2,
        0x04c1, 0x04cd, 1, 2,
        0x04d0, 0x04fe, 1, 2,
        0xff21, 0xff3a, 32, 1,
    };

    /* The first char of each run of ten decimal digits. */
    private static final char[] ZEROS = {'0', '٠', '۰', '०', '０'};

    private final char value;

    public Character(char value) {
        this.value = value;
    }

    public char charValue() {
        return value;
    }

    public int hashCode() {
        return value;
    }

    public boolean equals(Object obj) {
        return obj instanceof Character && ((Character) obj).value == value;
    }

    public String toString() {
        return String.valueOf(value);
    }

    public static boolean isUpperCase(char ch) {
        return toLowerCase(ch) != ch;
    }

    public static boolean isLowerCase(char ch) {
        /* sharp s is a lower case letter with no upper case in one char */
        return toUpperCase(ch) != ch || ch == 'ß';
    }

    public static char toLowerCase(char ch) {
        if (ch < 0x80) {
            return ch >= 'A' && ch <= 'Z' ? (char) (ch + 32) : ch;
        }
        for (int i = 0; i < CASES.length && ch >= CASES[i]; i += 4) {
            if (ch <= CASES[i + 1] && (ch - CASES[i]) % CASES[i + 3] == 0) {
                return (char) (ch + CASES[i + 2]);
            }
        }
        return ch;
    }

    public static char toUpperCase(char ch) {
        if (ch < 0x80) {
            return ch >= 'a' && ch <= 'z' ? (char) (ch - 32) : ch;
        }
        for (int i = 0; i < CASES.length; i += 4) {
            int c = ch - CASES[i + 2];
            if (c < CASES[i]) {
                continue;
            }
            if (c <= CASES[i + 1] && (c - CASES[i]) % CASES[i + 3] == 0) {
                return (char) c;
            }
        }
        if (ch == 'µ') {
            return 'Μ';   /* micro sign: Greek capital mu */
        }
        if (ch == 'ς') {
            return 'Σ';   /* final sigma */
        }
        return ch;
    }

    public static boolean isDigit(char ch) {
        return decimal(ch) >= 0;
    }

    /** The value of ch as a digit of radix, or -1 if it is none. */
    public static int digit(char ch, int radix) {
        if (radix < MIN_RADIX || radix > MAX_RADIX) {
            return -1;
        }
        int v = decimal(ch);
        if (v < 0) {
            if (ch >= 'a' && ch <= 'z') {
                v = ch - 'a' + 10;
            } else if (ch >= 'A' && ch <= 'Z') {
                v = ch - 'A' + 10;
            } else if (ch >= 'ａ' && ch <= 'ｚ') {
                v = ch - 'ａ' + 10;   /* fullwidth letters */
            } else if (ch >= 'Ａ' && ch <= 'Ｚ') {
                v = ch - 'Ａ' + 10;
            }
        }
        return v < radix ? v : -1;
    }

    private static int decimal(char ch) {
        for (int i = 0; i < ZEROS.length; i++) {
            if (ch >= ZEROS[i] && ch <= ZEROS[i] + 9) {
                return ch - ZEROS[i];
            }
        }
        return -1;
    }
}
