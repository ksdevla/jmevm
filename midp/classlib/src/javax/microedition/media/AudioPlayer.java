package javax.microedition.media;

import java.util.Vector;

import javax.microedition.media.control.ToneControl;
import javax.microedition.media.control.VolumeControl;

import jmevm.midp.Audio;

/**
 * A player of media held in memory (MIDI, WAV, tone sequences), or of a
 * tone sequence set through its ToneControl. The platform's synthesizer
 * makes the sound; the timing is kept here, in emulated time, so a player
 * behaves the same whether or not there is sound: media the synthesizer
 * cannot play lasts its own length silently, or one second when even that
 * is unknown.
 */
final class AudioPlayer implements Player {
    private static final long UNKNOWN_LENGTH = 1000000;   /* us */

    private byte[] data;
    private final String contentType;
    private final boolean tonePlayer;
    private int state = UNREALIZED;
    private long duration;
    private int loopCount = 1;
    private int loopsDone;
    private long mediaTime;    /* us, while not started; at startedAt while started */
    private long startedAt;    /* ms */
    private int handle;
    private int level = 100;
    private boolean muted;
    private final Vector listeners = new Vector();
    private final Volume volume = new Volume();
    private final Tones tones;

    AudioPlayer(byte[] data, String contentType, boolean tonePlayer) {
        this.data = data;
        this.contentType = contentType;
        this.tonePlayer = tonePlayer;
        this.tones = tonePlayer ? new Tones() : null;
        this.duration = data == null ? 0 : MediaLength.of(data, contentType);
    }

    // ---- state

    private void checkClosed() {
        if (state == CLOSED) {
            throw new IllegalStateException("the player is closed");
        }
    }

    private void checkRealized() {
        checkClosed();
        if (state == UNREALIZED) {
            throw new IllegalStateException("the player is not realized");
        }
    }

    private long length() {
        return duration != TIME_UNKNOWN ? duration : UNKNOWN_LENGTH;
    }

    private long now(long nowMs) {
        long t = mediaTime + (state == STARTED ? (nowMs - startedAt) * 1000 : 0);
        return Math.min(t, length());
    }

    public synchronized void realize() throws MediaException {
        checkClosed();
        if (state == UNREALIZED) {
            state = REALIZED;
        }
    }

    public synchronized void prefetch() throws MediaException {
        realize();
        if (state != REALIZED) {
            return;
        }
        if (handle == 0 && data != null && Audio.available()) {
            handle = Audio.open(data, contentType);
            if (handle != 0) {
                applyVolume();
                long d = Audio.duration(handle);
                if (duration == TIME_UNKNOWN && d >= 0) {   /* the media's own length counts first */
                    duration = d;
                    MediaThread.post(this, PlayerListener.DURATION_UPDATED, new Long(d));
                }
            }
        }
        state = PREFETCHED;
    }

    public synchronized void start() throws MediaException {
        prefetch();
        if (state == STARTED) {
            return;
        }
        if (mediaTime >= length()) {
            mediaTime = 0;   /* at the end: from the beginning */
        }
        startedAt = System.currentTimeMillis();
        state = STARTED;
        if (handle != 0) {
            Audio.start(handle, mediaTime);
        }
        MediaThread.post(this, PlayerListener.STARTED, new Long(mediaTime));
        MediaThread.timeChanged(this);
    }

    /** Stops the sound where it is; the caller posts the event. */
    private void halt() {
        mediaTime = now(System.currentTimeMillis());
        state = PREFETCHED;
        if (handle != 0) {
            Audio.stop(handle);
        }
        MediaThread.timeChanged(this);
    }

    public synchronized void stop() throws MediaException {
        checkClosed();
        if (state == STARTED) {
            halt();
            MediaThread.post(this, PlayerListener.STOPPED, new Long(mediaTime));
        }
    }

    public synchronized void deallocate() {
        checkClosed();
        if (state == STARTED) {
            halt();
            MediaThread.post(this, PlayerListener.STOPPED, new Long(mediaTime));
        }
        if (state == PREFETCHED) {
            if (handle != 0) {
                Audio.close(handle);
                handle = 0;
            }
            state = REALIZED;
        }
    }

    public synchronized void close() {
        if (state == CLOSED) {
            return;
        }
        if (state == STARTED) {
            halt();
        }
        if (handle != 0) {
            Audio.close(handle);
            handle = 0;
        }
        state = CLOSED;
        data = null;
        MediaThread.post(this, PlayerListener.CLOSED, null);
    }

    public synchronized long setMediaTime(long now) throws MediaException {
        checkRealized();
        now = Math.max(0, duration != TIME_UNKNOWN ? Math.min(now, duration) : now);
        mediaTime = now;
        if (state == STARTED) {
            startedAt = System.currentTimeMillis();
            if (handle != 0) {
                Audio.start(handle, now);
            }
            MediaThread.timeChanged(this);
        }
        return now;
    }

    public synchronized long getMediaTime() {
        checkClosed();
        return now(System.currentTimeMillis());
    }

    public synchronized int getState() {
        return state;
    }

    public synchronized long getDuration() {
        checkClosed();
        return duration;
    }

    public synchronized String getContentType() {
        checkRealized();
        return contentType;
    }

    public synchronized void setLoopCount(int count) {
        checkClosed();
        if (state == STARTED) {
            throw new IllegalStateException("the player is started");
        }
        if (count == 0 || count < -1) {
            throw new IllegalArgumentException("loop count " + count);
        }
        loopCount = count;
        loopsDone = 0;
    }

    /** For the media thread: when the current loop ends (ms), or -1. */
    synchronized long endAt() {
        if (state != STARTED) {
            return -1;
        }
        return startedAt + (length() - mediaTime + 999) / 1000;
    }

    /** For the media thread: the current loop has reached the end. */
    synchronized void end() {
        long now = System.currentTimeMillis();
        if (state != STARTED || now < endAt()) {
            return;
        }
        loopsDone++;
        if (loopCount == -1 || loopsDone < loopCount) {
            MediaThread.post(this, PlayerListener.END_OF_MEDIA, new Long(length()));
            mediaTime = 0;
            startedAt = now;
            if (handle != 0) {
                Audio.start(handle, 0);
            }
        } else {
            loopsDone = 0;
            mediaTime = length();
            state = PREFETCHED;
            if (handle != 0) {
                Audio.stop(handle);
            }
            MediaThread.post(this, PlayerListener.END_OF_MEDIA, new Long(length()));
        }
        MediaThread.timeChanged(this);
    }

    // ---- listeners

    public void addPlayerListener(PlayerListener l) {
        synchronized (this) {
            checkClosed();
        }
        synchronized (listeners) {
            if (l != null && !listeners.contains(l)) {
                listeners.addElement(l);
            }
        }
    }

    public void removePlayerListener(PlayerListener l) {
        synchronized (this) {
            checkClosed();
        }
        listeners.removeElement(l);
    }

    /** For the media thread: hands an event to the listeners. */
    void deliver(String event, Object eventData) {
        PlayerListener[] ls;
        synchronized (listeners) {
            ls = new PlayerListener[listeners.size()];
            listeners.copyInto(ls);
        }
        for (int i = 0; i < ls.length; i++) {
            ls[i].playerUpdate(this, event, eventData);
        }
    }

    // ---- controls

    public Control[] getControls() {
        synchronized (this) {
            checkRealized();
        }
        return tonePlayer ? new Control[] {volume, tones} : new Control[] {volume};
    }

    public Control getControl(String type) {
        synchronized (this) {
            checkRealized();
        }
        if (type == null) {
            throw new IllegalArgumentException("no control type");
        }
        String name = type.startsWith("javax.microedition.media.control.")
                ? type.substring("javax.microedition.media.control.".length()) : type;
        if (name.equals("VolumeControl")) {
            return volume;
        }
        if (name.equals("ToneControl") && tonePlayer) {
            return tones;
        }
        return null;
    }

    private void applyVolume() {
        if (handle != 0) {
            Audio.setVolume(handle, muted ? 0 : level);
        }
    }

    private final class Volume implements VolumeControl {
        public void setMute(boolean mute) {
            synchronized (AudioPlayer.this) {
                if (mute == muted) {
                    return;
                }
                muted = mute;
                applyVolume();
            }
            MediaThread.post(AudioPlayer.this, PlayerListener.VOLUME_CHANGED, this);
        }

        public boolean isMuted() {
            synchronized (AudioPlayer.this) {
                return muted;
            }
        }

        public int setLevel(int l) {
            int clamped = Math.max(0, Math.min(100, l));
            synchronized (AudioPlayer.this) {
                if (clamped == level) {
                    return level;
                }
                level = clamped;
                applyVolume();
            }
            MediaThread.post(AudioPlayer.this, PlayerListener.VOLUME_CHANGED, this);
            return clamped;
        }

        public int getLevel() {
            synchronized (AudioPlayer.this) {
                return level;
            }
        }
    }

    private final class Tones implements ToneControl {
        public void setSequence(byte[] sequence) {
            synchronized (AudioPlayer.this) {
                if (state == PREFETCHED || state == STARTED) {
                    throw new IllegalStateException("the player is prefetched");
                }
                checkClosed();
                long us = MediaLength.tones(sequence);
                if (us < 0) {
                    throw new IllegalArgumentException("not a valid tone sequence");
                }
                data = (byte[]) sequence.clone();
                duration = us;
            }
            MediaThread.post(AudioPlayer.this, PlayerListener.DURATION_UPDATED, new Long(duration));
        }
    }
}
