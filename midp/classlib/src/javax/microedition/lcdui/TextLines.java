package javax.microedition.lcdui;

import java.util.Vector;

/** Text broken into lines that fit a width: at newlines, between words,
 *  and inside words too long for a line. */
final class TextLines {
    private TextLines() {
    }

    static String[] wrap(String text, Font f, int width) {
        Vector lines = new Vector();
        if (text == null) {
            text = "";
        }
        width = Math.max(width, f.charWidth('W'));
        int start = 0;
        while (start <= text.length()) {
            int nl = text.indexOf('\n', start);
            String para = text.substring(start, nl < 0 ? text.length() : nl);
            wrapParagraph(para, f, width, lines);
            if (nl < 0) {
                break;
            }
            start = nl + 1;
        }
        String[] out = new String[lines.size()];
        lines.copyInto(out);
        return out;
    }

    private static void wrapParagraph(String p, Font f, int width, Vector lines) {
        if (p.length() == 0) {
            lines.addElement("");
            return;
        }
        int at = 0;
        while (at < p.length()) {
            int end = at, lastBreak = -1;
            while (end < p.length() && f.substringWidth(p, at, end - at + 1) <= width) {
                if (p.charAt(end) == ' ') {
                    lastBreak = end;
                }
                end++;
            }
            if (end == p.length()) {
                lines.addElement(p.substring(at));
                return;
            }
            if (lastBreak > at) {
                lines.addElement(p.substring(at, lastBreak));
                at = lastBreak + 1;
            } else {
                end = Math.max(end, at + 1);   /* a word wider than the line */
                lines.addElement(p.substring(at, end));
                at = end;
            }
        }
    }
}
