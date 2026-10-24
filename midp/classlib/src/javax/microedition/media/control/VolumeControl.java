package javax.microedition.media.control;

import javax.microedition.media.Control;

/** The volume of a player, 0 to 100, and muting. */
public interface VolumeControl extends Control {
    void setMute(boolean mute);

    boolean isMuted();

    int setLevel(int level);

    int getLevel();
}
