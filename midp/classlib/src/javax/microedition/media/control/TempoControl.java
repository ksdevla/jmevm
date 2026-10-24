package javax.microedition.media.control;

/** The tempo of a MIDI player, in milli-beats per minute (JSR 135). */
public interface TempoControl extends RateControl {
    int setTempo(int millitempo);

    int getTempo();
}
