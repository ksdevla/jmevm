package com.nokia.mid.sound;

import java.io.ByteArrayInputStream;

import javax.microedition.media.Manager;
import javax.microedition.media.Player;
import javax.microedition.media.PlayerListener;
import javax.microedition.media.control.VolumeControl;

/**
 * Nokia's simple sound: a tone of a frequency, a Smart Messaging ringtone,
 * or a WAV file. Played by a media player (tones and ringtones as MIDI).
 */
public class Sound {
    public static final int FORMAT_TONE = 1;
    public static final int FORMAT_WAV = 5;
    public static final int SOUND_PLAYING = 0;
    public static final int SOUND_STOPPED = 1;
    public static final int SOUND_UNINITIALIZED = 3;

    private Player player;
    private int state = SOUND_UNINITIALIZED;
    private int gain = 255;
    private SoundListener listener;

    public Sound(int freq, long duration) {
        init(freq, duration);
    }

    public Sound(byte[] data, int type) {
        init(data, type);
    }

    public static int getConcurrentSoundCount(int type) {
        return 1;
    }

    public static int[] getSupportedFormats() {
        return new int[] {FORMAT_TONE, FORMAT_WAV};
    }

    public void init(int freq, long duration) {
        if (freq < 0 || duration <= 0) {
            throw new IllegalArgumentException("tone of " + freq + " Hz for " + duration + " ms");
        }
        open(Tunes.tone(freq, duration), "audio/midi");
    }

    public void init(byte[] data, int type) {
        if (data == null) {
            throw new NullPointerException();
        }
        if (type == FORMAT_WAV) {
            open(data, "audio/x-wav");
        } else if (type == FORMAT_TONE) {
            byte[] midi = Tunes.ringtone(data);
            if (midi == null) {
                throw new IllegalArgumentException("not a ringtone");
            }
            open(midi, "audio/midi");
        } else {
            throw new IllegalArgumentException("format " + type);
        }
    }

    private synchronized void open(byte[] data, String type) {
        release();
        try {
            player = Manager.createPlayer(new ByteArrayInputStream(data), type);
            player.realize();
            player.addPlayerListener(new PlayerListener() {
                public void playerUpdate(Player p, String event, Object eventData) {
                    if (p != player) {
                        return;
                    }
                    if (event.equals(PlayerListener.STARTED)) {
                        tell(SOUND_PLAYING);
                    } else if ((event.equals(PlayerListener.END_OF_MEDIA) && p.getState() != Player.STARTED)
                            || event.equals(PlayerListener.STOPPED)) {
                        synchronized (Sound.this) {
                            if (state == SOUND_PLAYING) {
                                state = SOUND_STOPPED;
                            }
                        }
                        tell(SOUND_STOPPED);
                    }
                }
            });
            applyGain();
            state = SOUND_STOPPED;
        } catch (Exception e) {
            player = null;
            throw new IllegalArgumentException("cannot play this sound: " + e);
        }
    }

    private void tell(int event) {
        SoundListener l = listener;
        if (l != null) {
            l.soundStateChanged(this, event);
        }
    }

    private void applyGain() {
        VolumeControl v = player == null ? null : (VolumeControl) player.getControl("VolumeControl");
        if (v != null) {
            v.setLevel(gain * 100 / 255);
        }
    }

    /** Plays loop times (0: until stopped). */
    public synchronized void play(int loop) {
        if (loop < 0 || loop > 255) {
            throw new IllegalArgumentException("loop " + loop);
        }
        if (player == null) {
            throw new IllegalStateException("the sound was released");
        }
        try {
            player.stop();
            player.setMediaTime(0);
            player.setLoopCount(loop == 0 ? -1 : loop);
            player.start();
            state = SOUND_PLAYING;
        } catch (Exception e) {
            state = SOUND_STOPPED;
        }
    }

    public synchronized void stop() {
        if (player != null && state == SOUND_PLAYING) {
            try {
                player.stop();
            } catch (Exception ignored) {
            }
            state = SOUND_STOPPED;
        }
    }

    public synchronized void resume() {
        if (player != null && state == SOUND_STOPPED) {
            try {
                player.start();
                state = SOUND_PLAYING;
            } catch (Exception ignored) {
            }
        }
    }

    public synchronized void release() {
        if (player != null) {
            player.close();
            player = null;
            state = SOUND_UNINITIALIZED;
            tell(SOUND_UNINITIALIZED);
        }
    }

    public synchronized int getState() {
        return state;
    }

    public synchronized void setGain(int gain) {
        this.gain = Math.max(0, Math.min(255, gain));
        applyGain();
    }

    public synchronized int getGain() {
        return gain;
    }

    public void setSoundListener(SoundListener listener) {
        this.listener = listener;
    }
}
