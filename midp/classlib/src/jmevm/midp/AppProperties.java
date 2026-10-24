package jmevm.midp;

import java.io.UnsupportedEncodingException;
import java.util.Hashtable;

/**
 * The application's attributes: from its .jad when it has one, else from
 * the jar's manifest. Both are "Name: value" lines in UTF-8; in the manifest
 * a line starting with a space continues the one before.
 */
public final class AppProperties {
    private static Hashtable jadAttributes;
    private static Hashtable manifestAttributes;

    private AppProperties() {
    }

    public static synchronized String get(String name) {
        if (jadAttributes == null) {
            jadAttributes = parse(jad(), false);
            manifestAttributes = parse(Resources.read("META-INF/MANIFEST.MF"), true);
        }
        String v = (String) jadAttributes.get(name);
        return v != null ? v : (String) manifestAttributes.get(name);
    }

    private static native byte[] jad();

    static Hashtable parse(byte[] data, boolean continuations) {
        Hashtable out = new Hashtable();
        if (data == null) {
            return out;
        }
        String text;
        try {
            text = new String(data, "UTF-8");
        } catch (UnsupportedEncodingException e) {
            text = new String(data);
        }
        String key = null;
        StringBuffer value = null;
        int i = 0;
        while (i <= text.length()) {
            int end = i;
            while (end < text.length() && text.charAt(end) != '\n' && text.charAt(end) != '\r') {
                end++;
            }
            String line = text.substring(i, end);
            if (continuations && line.startsWith(" ") && key != null) {
                value.append(line.substring(1));
            } else {
                if (key != null) {
                    out.put(key, value.toString().trim());
                    key = null;
                }
                int colon = line.indexOf(':');
                if (colon > 0) {
                    key = line.substring(0, colon).trim();
                    value = new StringBuffer(line.substring(colon + 1));
                }
            }
            i = end + 1;
            if (end + 1 < text.length() && text.charAt(end) == '\r' && text.charAt(end + 1) == '\n') {
                i++;
            }
        }
        if (key != null) {
            out.put(key, value.toString().trim());
        }
        return out;
    }
}
