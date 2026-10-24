package com.nokia.mid.sound;

import java.io.ByteArrayOutputStream;

/**
 * Nokia's sounds as Standard MIDI Files, for the media players to play: a
 * tone of a frequency, and ringtones in the Smart Messaging format.
 *
 * <p>A Smart Messaging ringtone is a bit stream: commands (ringing tone
 * programming, sound), a song type and title, then patterns of
 * instructions: notes (value, duration, duration specifier), scale, style,
 * tempo and volume. Patterns repeat as their loop value says (15: forever,
 * played once here).
 */
final class Tunes {
    private static final int DIVISION = 480;   /* ticks per quarter note */
    private static final int PROGRAM = 80;     /* square lead: a phone's beeper */
    private static final int[] BPM = {25, 28, 31, 35, 40, 45, 50, 56, 63, 70, 80, 90, 100, 112, 125, 140, 160, 180,
            200, 225, 250, 285, 320, 355, 400, 450, 500, 565, 635, 715, 800, 900};

    private Tunes() {
    }

    /** The MIDI note nearest a frequency (Hz). */
    static int noteOf(int freq) {
        int note = 69;
        double f = 440.0;
        double up = 1.0594630943592953;   /* 2^(1/12) */
        while (note < 127 && freq > f * Math.sqrt(up)) {
            f *= up;
            note++;
        }
        while (note > 0 && freq < f / Math.sqrt(up)) {
            f /= up;
            note--;
        }
        return note;
    }

    /** One note of a frequency for a time, at 120 beats a minute. */
    static byte[] tone(int freq, long durationMs) {
        Midi m = new Midi(500000);
        long ticks = durationMs * DIVISION / 500;
        if (freq <= 0) {
            m.rest(ticks);
        } else {
            m.note(noteOf(freq), 100, ticks);
        }
        return m.bytes();
    }

    /** A Smart Messaging ringtone, or null if it is not one. */
    static byte[] ringtone(byte[] data) {
        try {
            return parse(new Bits(data));
        } catch (RuntimeException e) {
            return null;
        }
    }

    private static byte[] parse(Bits in) {
        /* Commands of 7 bits and a filler bit, but for "sound", which the
         * song follows at once. */
        int commands = in.read(8);
        boolean unicode = false, song = false;
        for (int i = 0; i < commands && !song; i++) {
            int c = in.read(7);
            if (c == 0x1D) {
                song = true;
            } else {
                unicode |= c == 0x22;
                in.read(1);
            }
        }
        if (!song) {
            return null;
        }
        int type = in.read(3);
        if (type != 1 && type != 2) {   /* basic or temporary song */
            return null;
        }
        int titleLength = in.read(4);
        in.skip(titleLength * (unicode ? 16 : 8));
        int patterns = in.read(8);
        int bpm = 63, volume = 7, scale = 1;
        int[] durations = {4 * DIVISION, 2 * DIVISION, DIVISION, DIVISION / 2, DIVISION / 4, DIVISION / 8};
        Midi m = new Midi(60000000 / bpm);
        for (int p = 0; p < patterns; p++) {
            in.read(3);   /* pattern header id */
            in.read(2);   /* pattern id */
            int loops = in.read(4);
            int count = in.read(8);
            int start = in.position();
            int repeats = loops == 0 || loops == 15 ? 1 : loops + 1;
            for (int r = 0; r < repeats; r++) {
                in.seek(start);
                for (int i = 0; i < count; i++) {
                    int id = in.read(3);
                    if (id == 1) {   /* note */
                        int value = in.read(4), dur = in.read(3), spec = in.read(2);
                        long ticks = dur < durations.length ? durations[dur] : DIVISION;
                        ticks = spec == 1 ? ticks * 3 / 2 : spec == 2 ? ticks * 7 / 4 : spec == 3 ? ticks * 2 / 3 : ticks;
                        if (value == 0 || value > 12) {
                            m.rest(ticks);
                        } else {
                            /* scale 0 is the octave of A = 440 Hz: its C is MIDI note 60 */
                            m.note(60 + 12 * scale + value - 1, volume * 127 / 15, ticks);
                        }
                    } else if (id == 2) {
                        scale = in.read(2);
                    } else if (id == 3) {
                        in.read(2);   /* style */
                    } else if (id == 4) {
                        m.tempo(60000000 / BPM[in.read(5)]);
                    } else if (id == 5) {
                        volume = in.read(4);
                    } else {
                        break;
                    }
                }
            }
        }
        return m.bytes();
    }

    /** Reads bits, most significant first. */
    private static final class Bits {
        private final byte[] data;
        private int bit;

        Bits(byte[] data) {
            this.data = data;
        }

        int read(int n) {
            int v = 0;
            for (int i = 0; i < n; i++, bit++) {
                if (bit / 8 >= data.length) {
                    throw new IllegalArgumentException("truncated");
                }
                v = (v << 1) | ((data[bit / 8] >> (7 - bit % 8)) & 1);
            }
            return v;
        }

        void skip(int n) {
            bit += n;
        }

        int position() {
            return bit;
        }

        void seek(int position) {
            bit = position;
        }
    }

    /** A format 0 MIDI file on channel 1, written as it is built. */
    private static final class Midi {
        private final ByteArrayOutputStream track = new ByteArrayOutputStream();
        private long pending;   /* ticks since the last event */

        Midi(int usPerQuarter) {
            tempo(usPerQuarter);
            event(new byte[] {(byte) 0xC0, (byte) PROGRAM});
        }

        private void delta() {
            long v = pending;
            pending = 0;
            int[] groups = new int[5];
            int n = 0;
            do {
                groups[n++] = (int) (v & 0x7F);
                v >>= 7;
            } while (v > 0 && n < 5);
            for (int i = n - 1; i >= 0; i--) {
                track.write(groups[i] | (i > 0 ? 0x80 : 0));
            }
        }

        private void event(byte[] e) {
            delta();
            track.write(e, 0, e.length);
        }

        void tempo(int usPerQuarter) {
            event(new byte[] {(byte) 0xFF, 0x51, 3, (byte) (usPerQuarter >> 16), (byte) (usPerQuarter >> 8),
                    (byte) usPerQuarter});
        }

        void note(int note, int velocity, long ticks) {
            event(new byte[] {(byte) 0x90, (byte) note, (byte) Math.max(1, velocity)});
            pending += ticks;
            event(new byte[] {(byte) 0x80, (byte) note, 64});
        }

        void rest(long ticks) {
            pending += ticks;
        }

        byte[] bytes() {
            event(new byte[] {(byte) 0xFF, 0x2F, 0});
            byte[] body = track.toByteArray();
            ByteArrayOutputStream out = new ByteArrayOutputStream(22 + body.length);
            byte[] header = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, (byte) (DIVISION >> 8), (byte) DIVISION,
                    'M', 'T', 'r', 'k', (byte) (body.length >> 24), (byte) (body.length >> 16),
                    (byte) (body.length >> 8), (byte) body.length};
            out.write(header, 0, header.length);
            out.write(body, 0, body.length);
            return out.toByteArray();
        }
    }
}
