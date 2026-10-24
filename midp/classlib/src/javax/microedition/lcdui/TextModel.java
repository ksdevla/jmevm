package javax.microedition.lcdui;

/**
 * The text of a TextField or TextBox: its constraints, maximum size and
 * caret, and phone-style typing on the number keys.
 *
 * <p>Free text is typed by tapping 2-9 (and 1 for punctuation, 0 for a
 * space) until the wanted character shows; another key, or a second's
 * pause, keeps it. '*' switches capitals, '#' types a space. Numeric text
 * takes the digits as they are, '*' for the sign and '#' for the decimal
 * point or (phone numbers) '#'. Clear deletes, the side arrows move the
 * caret.
 */
final class TextModel {
    private static final String[] TAPS = {
        " 0", ".,?!'\"1-()@/:_", "abc2", "def3", "ghi4", "jkl5", "mno6", "pqrs7", "tuv8", "wxyz9"};
    private static final int TAP_TIMEOUT = 1000;
    private static final int MODIFIERS = TextField.PASSWORD | TextField.UNEDITABLE | TextField.SENSITIVE
            | TextField.NON_PREDICTIVE | TextField.INITIAL_CAPS_WORD | TextField.INITIAL_CAPS_SENTENCE;

    private final StringBuffer text = new StringBuffer();
    int max;
    int constraints;
    int caret;
    private boolean capitals;
    private int tapKey;
    private int tapIndex;
    private long tapAt;

    TextModel(String initial, int maxSize, int constraints) {
        checkConstraints(constraints);
        if (maxSize <= 0) {
            throw new IllegalArgumentException("maximum size " + maxSize);
        }
        this.max = maxSize;
        this.constraints = constraints;
        set(initial);
    }

    static void checkConstraints(int c) {
        int kind = c & TextField.CONSTRAINT_MASK;
        if (kind > TextField.DECIMAL || (c & ~TextField.CONSTRAINT_MASK & ~MODIFIERS) != 0) {
            throw new IllegalArgumentException("constraints " + c);
        }
    }

    /** Whether s is allowed under constraints c. */
    static boolean allowed(String s, int c) {
        int kind = c & TextField.CONSTRAINT_MASK;
        int start = (kind == TextField.NUMERIC || kind == TextField.DECIMAL) && s.startsWith("-") ? 1 : 0;
        boolean point = false;
        for (int i = start; i < s.length(); i++) {
            char ch = s.charAt(i);
            boolean digit = ch >= '0' && ch <= '9';
            if (kind == TextField.NUMERIC && !digit) {
                return false;
            }
            if (kind == TextField.DECIMAL && !digit) {
                if (ch != '.' || point) {
                    return false;
                }
                point = true;
            }
            if (kind == TextField.PHONENUMBER && !digit && "+*#pw ".indexOf(ch) < 0) {
                return false;
            }
        }
        return true;
    }

    String get() {
        return text.toString();
    }

    int size() {
        return text.length();
    }

    void set(String s) {
        if (s == null) {
            s = "";
        }
        if (s.length() > max) {
            throw new IllegalArgumentException("text longer than " + max);
        }
        if (!allowed(s, constraints)) {
            throw new IllegalArgumentException("text not allowed by the constraints");
        }
        text.setLength(0);
        text.append(s);
        caret = s.length();
        tapKey = 0;
    }

    void insert(String s, int position) {
        position = Math.max(0, Math.min(position, text.length()));
        String result = text.toString().substring(0, position) + s + text.toString().substring(position);
        set(result);
        caret = position + s.length();
    }

    void delete(int offset, int length) {
        if (offset < 0 || length < 0 || offset > text.length() - length) {
            throw new StringIndexOutOfBoundsException();
        }
        String result = text.toString().substring(0, offset) + text.toString().substring(offset + length);
        if (!allowed(result, constraints)) {
            throw new IllegalArgumentException("text not allowed by the constraints");
        }
        text.setLength(0);
        text.append(result);
        caret = Math.min(caret > offset ? Math.max(offset, caret - length) : caret, text.length());
    }

    int setMax(int maxSize) {
        if (maxSize <= 0) {
            throw new IllegalArgumentException("maximum size " + maxSize);
        }
        if (text.length() > maxSize) {
            String cut = text.toString().substring(0, maxSize);
            if (!allowed(cut, constraints)) {
                throw new IllegalArgumentException("the shortened text is not allowed");
            }
            text.setLength(maxSize);
        }
        max = maxSize;
        caret = Math.min(caret, text.length());
        return max;
    }

    void setConstraints(int c) {
        checkConstraints(c);
        constraints = c;
        if (!allowed(text.toString(), c)) {
            text.setLength(0);
            caret = 0;
        }
    }

    /** The text as shown: dots for a password. */
    String shown() {
        if ((constraints & TextField.PASSWORD) == 0) {
            return text.toString();
        }
        StringBuffer b = new StringBuffer(text.length());
        for (int i = 0; i < text.length(); i++) {
            b.append('*');
        }
        return b.toString();
    }

    // ---- typing

    /** Puts s at the caret if the result is allowed; false if not. */
    private boolean put(String s, boolean replacePrevious) {
        int at = replacePrevious ? caret - 1 : caret;
        String before = text.toString().substring(0, at), after = text.toString().substring(caret);
        String result = before + s + after;
        if (result.length() > max || !allowed(result, constraints)) {
            return false;
        }
        text.setLength(0);
        text.append(result);
        caret = at + s.length();
        return true;
    }

    /**
     * A key down or repeat: 0 if typing does not use it, 1 if used without
     * changing the text, 2 if the text changed.
     */
    int key(int keyCode, long now) {
        if ((constraints & TextField.UNEDITABLE) != 0) {
            return 0;
        }
        int kind = constraints & TextField.CONSTRAINT_MASK;
        boolean numeric = kind == TextField.NUMERIC || kind == TextField.DECIMAL || kind == TextField.PHONENUMBER;
        if (keyCode == Canvas.KEY_CLEAR) {
            tapKey = 0;
            if (caret == 0) {
                return 1;
            }
            delete(caret - 1, 1);
            return 2;
        }
        if (keyCode == Canvas.KEY_LEFT_ARROW || keyCode == Canvas.KEY_RIGHT_ARROW) {
            tapKey = 0;
            int c = Math.max(0, Math.min(text.length(), caret + (keyCode == Canvas.KEY_LEFT_ARROW ? -1 : 1)));
            if (c == caret) {
                return 0;   /* at the end: the form may use the key */
            }
            caret = c;
            return 1;
        }
        if (keyCode == Canvas.KEY_STAR) {
            tapKey = 0;
            if (kind == TextField.NUMERIC || kind == TextField.DECIMAL) {
                String t = text.toString();
                String toggled = t.startsWith("-") ? t.substring(1) : "-" + t;
                if (toggled.length() <= max) {
                    int shift = toggled.length() - t.length();
                    text.setLength(0);
                    text.append(toggled);
                    caret = Math.max(0, caret + shift);
                    return 2;
                }
                return 1;
            }
            if (kind == TextField.PHONENUMBER) {
                return put("*", false) ? 2 : 1;
            }
            capitals = !capitals;
            return 1;
        }
        if (keyCode == Canvas.KEY_POUND) {
            tapKey = 0;
            String s = kind == TextField.DECIMAL ? "." : kind == TextField.PHONENUMBER ? "#" : numeric ? "" : " ";
            return s.length() > 0 && put(s, false) ? 2 : 1;
        }
        if (keyCode < Canvas.KEY_NUM0 || keyCode > Canvas.KEY_NUM9) {
            return 0;
        }
        if (numeric) {
            tapKey = 0;
            return put(String.valueOf((char) keyCode), false) ? 2 : 1;
        }
        String cycle = TAPS[keyCode - Canvas.KEY_NUM0];
        boolean again = keyCode == tapKey && now - tapAt < TAP_TIMEOUT && caret > 0;
        tapIndex = again ? (tapIndex + 1) % cycle.length() : 0;
        char ch = cycle.charAt(tapIndex);
        if (capitals) {
            ch = Character.toUpperCase(ch);
        }
        boolean ok = put(String.valueOf(ch), again);
        tapKey = ok ? keyCode : 0;
        tapAt = now;
        return ok ? 2 : 1;
    }
}
