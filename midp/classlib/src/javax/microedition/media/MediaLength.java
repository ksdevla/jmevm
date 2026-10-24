package javax.microedition.media;

import javax.microedition.media.control.ToneControl;

/**
 * Durations of media from their bytes, in microseconds: Standard MIDI Files
 * (to the end of the last track, through the tempo map), WAV (PCM, mu-law,
 * A-law, IMA ADPCM), and tone sequences, which are also checked here.
 */
final class MediaLength {
    private MediaLength() {
    }

    static long of(byte[] data, String type) {
        if (data == null) {
            return Player.TIME_UNKNOWN;
        }
        if (startsWith(data, "MThd")) {
            return midi(data);
        }
        if (startsWith(data, "RIFF") && data.length >= 12 && data[8] == 'W' && data[9] == 'A') {
            return wav(data);
        }
        if (isTones(data, type)) {
            long us = tones(data);
            return us < 0 ? Player.TIME_UNKNOWN : us;
        }
        return Player.TIME_UNKNOWN;
    }

    static boolean isTones(byte[] data, String type) {
        return (type != null && type.indexOf("tone") >= 0)
                || (data.length >= 2 && data[0] == ToneControl.VERSION && data[1] == 1);
    }

    static boolean startsWith(byte[] d, String magic) {
        if (d.length < magic.length()) {
            return false;
        }
        for (int i = 0; i < magic.length(); i++) {
            if (d[i] != magic.charAt(i)) {
                return false;
            }
        }
        return true;
    }

    private static int be32(byte[] d, int at) {
        return ((d[at] & 0xFF) << 24) | ((d[at + 1] & 0xFF) << 16) | ((d[at + 2] & 0xFF) << 8) | (d[at + 3] & 0xFF);
    }

    private static int le16(byte[] d, int at) {
        return (d[at] & 0xFF) | ((d[at + 1] & 0xFF) << 8);
    }

    private static int le32(byte[] d, int at) {
        return le16(d, at) | (le16(d, at + 2) << 16);
    }

    // ---- MIDI

    private static long midi(byte[] d) {
        if (d.length < 14) {
            return Player.TIME_UNKNOWN;
        }
        int tracks = ((d[10] & 0xFF) << 8) | (d[11] & 0xFF);
        int division = ((d[12] & 0xFF) << 8) | (d[13] & 0xFF);
        if (division == 0) {
            return Player.TIME_UNKNOWN;
        }
        long[] tempoTicks = new long[16];
        int[] tempos = new int[16];
        int nTempo = 0;
        long end = 0;
        int pos = 8 + be32(d, 4);
        for (int t = 0; t < tracks && pos >= 0 && pos + 8 <= d.length; t++) {
            int size = be32(d, pos + 4);
            int p = pos + 8;
            int stop = size >= 0 && size <= d.length - p ? p + size : d.length;
            if (!startsWith(sub(d, pos), "MTrk")) {
                pos = stop;
                t--;
                continue;
            }
            long tick = 0;
            int status = 0;
            while (p < stop) {
                int delta = 0, b;
                int k = 0;
                do {
                    if (p >= stop || k++ == 4) {
                        p = stop;
                        break;
                    }
                    b = d[p++] & 0xFF;
                    delta = (delta << 7) | (b & 0x7F);
                } while ((b & 0x80) != 0);
                if (p >= stop) {
                    break;
                }
                tick += delta;
                int s = d[p] & 0xFF;
                if (s == 0xFF) {
                    if (p + 2 >= stop) {
                        break;
                    }
                    int type = d[p + 1] & 0xFF;
                    p += 2;
                    int len = 0;
                    do {
                        b = d[p++] & 0xFF;
                        len = (len << 7) | (b & 0x7F);
                    } while ((b & 0x80) != 0 && p < stop);
                    if (type == 0x51 && len == 3 && p + 3 <= stop) {
                        if (nTempo == tempos.length) {
                            tempoTicks = grow(tempoTicks);
                            tempos = grow(tempos);
                        }
                        tempoTicks[nTempo] = tick;
                        tempos[nTempo++] = ((d[p] & 0xFF) << 16) | ((d[p + 1] & 0xFF) << 8) | (d[p + 2] & 0xFF);
                    }
                    p += len;
                    if (type == 0x2F) {
                        break;
                    }
                } else if (s == 0xF0 || s == 0xF7) {
                    p++;
                    int len = 0;
                    do {
                        b = d[p++] & 0xFF;
                        len = (len << 7) | (b & 0x7F);
                    } while ((b & 0x80) != 0 && p < stop);
                    p += len;
                } else {
                    if ((s & 0x80) != 0) {
                        status = s;
                        p++;
                    } else if (status == 0) {
                        break;
                    }
                    p += (status & 0xE0) == 0xC0 ? 1 : 2;
                }
            }
            if (tick > end) {
                end = tick;
            }
            pos = stop;
        }
        if ((division & 0x8000) != 0) {   /* SMPTE: frames a second, ticks a frame */
            int fps = -(byte) (division >> 8);
            int perFrame = division & 0xFF;
            return end * 1000000L / ((fps > 0 ? fps : 25) * (perFrame > 0 ? perFrame : 1));
        }
        sortTempos(tempoTicks, tempos, nTempo);
        long us = 0, at = 0;
        int perQuarter = 500000;
        for (int i = 0; i < nTempo && tempoTicks[i] <= end; i++) {
            us += (tempoTicks[i] - at) * perQuarter / division;
            at = tempoTicks[i];
            if (tempos[i] > 0) {
                perQuarter = tempos[i];
            }
        }
        return us + (end - at) * perQuarter / division;
    }

    private static byte[] sub(byte[] d, int at) {
        byte[] s = new byte[Math.min(4, d.length - at)];
        System.arraycopy(d, at, s, 0, s.length);
        return s;
    }

    private static void sortTempos(long[] ticks, int[] values, int n) {
        for (int i = 1; i < n; i++) {   /* stable: tempo changes on one tick keep their order */
            long t = ticks[i];
            int v = values[i];
            int j = i - 1;
            while (j >= 0 && ticks[j] > t) {
                ticks[j + 1] = ticks[j];
                values[j + 1] = values[j];
                j--;
            }
            ticks[j + 1] = t;
            values[j + 1] = v;
        }
    }

    private static long[] grow(long[] a) {
        long[] b = new long[a.length * 2];
        System.arraycopy(a, 0, b, 0, a.length);
        return b;
    }

    private static int[] grow(int[] a) {
        int[] b = new int[a.length * 2];
        System.arraycopy(a, 0, b, 0, a.length);
        return b;
    }

    // ---- WAV

    private static long wav(byte[] d) {
        int format = 0, channels = 0, rate = 0, block = 0, bits = 0;
        for (int p = 12; p + 8 <= d.length;) {
            int size = le32(d, p + 4);
            boolean fmt = d[p] == 'f' && d[p + 1] == 'm' && d[p + 2] == 't';
            if (fmt && size >= 16 && p + 24 <= d.length) {
                format = le16(d, p + 8);
                channels = le16(d, p + 10);
                rate = le32(d, p + 12);
                block = le16(d, p + 20);
                bits = le16(d, p + 22);
            } else if (d[p] == 'd' && d[p + 1] == 'a' && d[p + 2] == 't' && d[p + 3] == 'a') {
                long bytes = Math.min(size & 0xFFFFFFFFL, d.length - p - 8);
                if (channels <= 0 || rate <= 0) {
                    return Player.TIME_UNKNOWN;
                }
                long samples;
                if (format == 1 && bits > 0) {
                    samples = bytes / (channels * ((bits + 7) / 8));
                } else if (format == 6 || format == 7) {
                    samples = bytes / channels;
                } else if (format == 0x11 && block > 4 * channels) {
                    long perBlock = (block - 4L * channels) * 2 / channels + 1;
                    long rest = bytes % block;
                    samples = bytes / block * perBlock
                            + (rest >= 4L * channels ? (rest - 4L * channels) * 2 / channels + 1 : 0);
                } else {
                    return Player.TIME_UNKNOWN;
                }
                return samples * 1000000L / rate;
            }
            if (size < 0 || size > d.length - p - 8) {
                break;
            }
            p += 8 + size + (size & 1);
        }
        return Player.TIME_UNKNOWN;
    }

    // ---- tone sequences

    /**
     * The length of a tone sequence, or -1 if it is not one. A sequence is
     * VERSION 1, then optionally TEMPO and RESOLUTION, then block
     * definitions, then events: notes and silences (with positive
     * durations), PLAY_BLOCK of a defined block, SET_VOLUME (0-100), and
     * REPEAT (2 or more) of the note or silence that follows.
     */
    static long tones(byte[] s) {
        if (s == null || s.length < 2 || s.length % 2 != 0 || s[0] != ToneControl.VERSION || s[1] != 1) {
            return -1;
        }
        int tempo = 30, resolution = 64;   /* 120 beats a minute, 1/64 notes */
        int[] blockStart = new int[128];
        int[] blockEnd = new int[128];
        int p = 2;
        while (p < s.length) {
            int a = s[p], b = s[p + 1];
            if (a == ToneControl.TEMPO && p == 2 && b >= 5) {
                tempo = b;
            } else if (a == ToneControl.RESOLUTION && b >= 1) {
                resolution = b;
            } else if (a == ToneControl.BLOCK_START && b >= 0) {
                int q = p + 2;
                while (q < s.length && !(s[q] == ToneControl.BLOCK_END && s[q + 1] == b)) {
                    if (s[q] == ToneControl.BLOCK_START) {
                        return -1;   /* blocks are not nested */
                    }
                    q += 2;
                }
                if (q >= s.length) {
                    return -1;
                }
                blockStart[b] = p + 2;
                blockEnd[b] = q;
                p = q;
            } else if (a == ToneControl.TEMPO || a == ToneControl.RESOLUTION || a == ToneControl.BLOCK_START) {
                return -1;
            } else {
                break;
            }
            p += 2;
        }
        long[] units = new long[1];   /* in 1/resolution notes */
        if (!play(s, p, s.length, blockStart, blockEnd, units, 0)) {
            return -1;
        }
        return units[0] * 60000000L * 4 / ((long) tempo * 4 * resolution);
    }

    private static boolean play(byte[] s, int from, int to, int[] blockStart, int[] blockEnd, long[] units,
            int depth) {
        if (depth > 16) {
            return false;
        }
        for (int p = from; p < to; p += 2) {
            int a = s[p], b = s[p + 1];
            if (a >= 0 || a == ToneControl.SILENCE) {
                if (b <= 0) {
                    return false;
                }
                units[0] += b;
            } else if (a == ToneControl.SET_VOLUME) {
                if (b < 0 || b > 100) {
                    return false;
                }
            } else if (a == ToneControl.REPEAT) {
                if (b < 2 || p + 3 >= to || s[p + 2] < ToneControl.SILENCE || s[p + 3] <= 0) {
                    return false;
                }
                units[0] += (long) b * s[p + 3];
                p += 2;
            } else if (a == ToneControl.PLAY_BLOCK) {
                if (b < 0 || blockEnd[b] == 0
                        || !play(s, blockStart[b], blockEnd[b], blockStart, blockEnd, units, depth + 1)) {
                    return false;
                }
            } else {
                return false;
            }
        }
        return true;
    }
}
