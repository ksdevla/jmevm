package jmevm.midp;

/**
 * The platform's sound (natives in src/midp_jme/audio.c). Media is decoded
 * and played natively; handles name opened media. Times are microseconds,
 * volumes 0-100.
 */
public final class Audio {
    private Audio() {
    }

    /** Whether there is sound at all. */
    public static native boolean available();

    /** Plays a note (MIDI number) for a time, without a player. */
    public static native void playTone(int note, int durationMs, int volume);

    /** Opens media (MIDI, WAV, tone sequence); 0 if it cannot be played. */
    public static native int open(byte[] data, String contentType);

    /** The length of opened media, or -1 if unknown. */
    public static native long duration(int handle);

    public static native void start(int handle, long mediaTime);

    public static native void stop(int handle);

    public static native void setVolume(int handle, int level);

    public static native void close(int handle);
}
