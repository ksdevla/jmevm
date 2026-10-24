package javax.microedition.media;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;

import jmevm.midp.Audio;

/**
 * Creates players. Media can come from a stream (MIDI, WAV and tone
 * sequences play; other types make silent players of their own length, or
 * of a second, so games that ship MP3 or AMR go on) or from the tone device.
 */
public final class Manager {
    public static final String TONE_DEVICE_LOCATOR = "device://tone";
    public static final String MIDI_DEVICE_LOCATOR = "device://midi";

    private static final String MIDI = "audio/midi";
    private static final String WAV = "audio/x-wav";
    private static final String TONES = "audio/x-tone-seq";

    private Manager() {
    }

    public static String[] getSupportedContentTypes(String protocol) {
        if (protocol == null) {
            return new String[] {MIDI, WAV, TONES};
        }
        if (protocol.equals("device")) {
            return new String[] {TONES};
        }
        return new String[0];
    }

    public static String[] getSupportedProtocols(String contentType) {
        if (contentType == null || contentType.equals(TONES)) {
            return new String[] {"device"};
        }
        return new String[0];
    }

    public static Player createPlayer(String locator) throws IOException, MediaException {
        if (locator == null) {
            throw new IllegalArgumentException("no locator");
        }
        if (locator.equals(TONE_DEVICE_LOCATOR)) {
            return new AudioPlayer(null, TONES, true);
        }
        throw new MediaException("cannot play " + locator);
    }

    public static Player createPlayer(InputStream stream, String type) throws IOException, MediaException {
        if (stream == null) {
            throw new IllegalArgumentException("no stream");
        }
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        byte[] buf = new byte[4096];
        for (int n; (n = stream.read(buf)) > 0;) {
            bytes.write(buf, 0, n);
        }
        byte[] data = bytes.toByteArray();
        if (type == null) {
            type = sniff(data);
            if (type == null) {
                throw new MediaException("unknown media type");
            }
        }
        return new AudioPlayer(data, type, MediaLength.isTones(data, type));
    }

    /** The type of media from its first bytes, or null. */
    private static String sniff(byte[] data) {
        if (MediaLength.startsWith(data, "MThd")) {
            return MIDI;
        }
        if (MediaLength.startsWith(data, "RIFF")) {
            return WAV;
        }
        if (MediaLength.isTones(data, null)) {
            return TONES;
        }
        return null;
    }

    public static void playTone(int note, int duration, int volume) throws MediaException {
        if (note < 0 || note > 127) {
            throw new IllegalArgumentException("note " + note);
        }
        if (duration <= 0) {
            throw new IllegalArgumentException("duration " + duration);
        }
        if (Audio.available()) {
            Audio.playTone(note, duration, Math.max(0, Math.min(100, volume)));
        }
    }
}
