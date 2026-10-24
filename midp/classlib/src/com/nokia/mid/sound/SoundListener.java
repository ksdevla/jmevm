package com.nokia.mid.sound;

/** Hears about a sound starting, stopping or being released. */
public interface SoundListener {
    void soundStateChanged(Sound sound, int event);
}
