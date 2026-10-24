package jmevm.test.sound;

import java.io.ByteArrayInputStream;
import java.io.IOException;

import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.media.Manager;
import javax.microedition.media.MediaException;
import javax.microedition.media.Player;
import javax.microedition.media.PlayerListener;
import javax.microedition.media.control.ToneControl;
import javax.microedition.media.control.VolumeControl;
import javax.microedition.midlet.MIDlet;

/**
 * Sound (JSR 135): MIDI, WAV and tone-sequence players, built from bytes
 * made here, so their durations are known. The Player state machine, its
 * events, volume, looping, media time, closing, and Manager.playTone, all
 * against the specification. A canvas shows what is playing.
 */
public class SoundMIDlet extends MIDlet {
    static volatile String status = "starting";

    protected void startApp() {
        final Canvas screen = new Canvas() {
            protected void paint(Graphics g) {
                g.setColor(0x202020);
                g.fillRect(0, 0, getWidth(), getHeight());
                g.setColor(0x80FF80);
                g.drawString("jmevm sound", 4, 4, Graphics.TOP | Graphics.LEFT);
                g.drawString(status, 4, 24, Graphics.TOP | Graphics.LEFT);
            }
        };
        Display.getDisplay(this).setCurrent(screen);
        new Thread(new Runnable() {
            public void run() {
                try {
                    checks(screen);
                } catch (Throwable t) {
                    Check.yes("no exception: " + t, false);
                }
                Check.summary();
                status = "done";
                screen.repaint();
            }
        }).start();
    }

    protected void pauseApp() {
    }

    protected void destroyApp(boolean unconditional) {
    }

    // ---- media made here

    static void put16(byte[] b, int at, int v) {
        b[at] = (byte) v;
        b[at + 1] = (byte) (v >> 8);
    }

    static void put32(byte[] b, int at, int v) {
        put16(b, at, v);
        put16(b, at + 2, v >> 16);
    }

    /** A format 0 MIDI file: 480 ticks per quarter note, 500000 us per quarter
     * note, one note held for the given number of ticks. */
    static byte[] midi(int ticks) {
        byte[] track = {
            0, (byte) 0xFF, 0x51, 3, 0x07, (byte) 0xA1, 0x20,   // tempo 500000 us
            0, (byte) 0xC0, 0,                                  // program 0
            0, (byte) 0x90, 60, 100,                            // note on, middle C
            (byte) (0x80 | (ticks >> 7)), (byte) (ticks & 0x7F),   // delta (two-byte VLQ)
            (byte) 0x80, 60, 64,                                // note off
            0, (byte) 0xFF, 0x2F, 0};                           // end of track
        byte[] header = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0x01, (byte) 0xE0,
                'M', 'T', 'r', 'k', 0, 0, 0, (byte) track.length};
        byte[] b = new byte[header.length + track.length];
        System.arraycopy(header, 0, b, 0, header.length);
        System.arraycopy(track, 0, b, header.length, track.length);
        return b;
    }

    /** A 16-bit mono WAV file at 8000 Hz with the given number of samples. */
    static byte[] wav(int samples) {
        byte[] b = new byte[44 + samples * 2];
        b[0] = 'R'; b[1] = 'I'; b[2] = 'F'; b[3] = 'F';
        put32(b, 4, 36 + samples * 2);
        b[8] = 'W'; b[9] = 'A'; b[10] = 'V'; b[11] = 'E';
        b[12] = 'f'; b[13] = 'm'; b[14] = 't'; b[15] = ' ';
        put32(b, 16, 16);
        put16(b, 20, 1);        // PCM
        put16(b, 22, 1);        // mono
        put32(b, 24, 8000);
        put32(b, 28, 16000);    // bytes per second
        put16(b, 32, 2);        // block align
        put16(b, 34, 16);       // bits per sample
        b[36] = 'd'; b[37] = 'a'; b[38] = 't'; b[39] = 'a';
        put32(b, 40, samples * 2);
        for (int i = 0; i < samples; i++) {
            put16(b, 44 + i * 2, (i / 10) % 2 == 0 ? 6000 : -6000);   // 400 Hz square wave
        }
        return b;
    }

    static Player player(byte[] data, String type) throws IOException, MediaException {
        return Manager.createPlayer(new ByteArrayInputStream(data), type);
    }

    static void show(Canvas screen, String what) {
        status = what;
        screen.repaint();
    }

    // ---- checks

    static void checks(Canvas screen) throws Exception {
        String[] types = Manager.getSupportedContentTypes(null);
        boolean midiType = false, wavType = false;
        for (int i = 0; i < types.length; i++) {
            midiType |= types[i].equals("audio/midi");
            wavType |= types[i].equals("audio/x-wav");
        }
        Check.yes("supported content types", midiType && wavType);

        // ---- the state machine, with a MIDI player
        final Player midi = player(midi(480), "audio/midi");
        Check.eq("new player is unrealized", midi.getState(), Player.UNREALIZED);
        Check.fails("content type needs realize", IllegalStateException.class, new Check.Action() {
            public void run() {
                midi.getContentType();
            }
        });
        midi.realize();
        Check.eq("realized", midi.getState(), Player.REALIZED);
        Check.eq("midi content type", midi.getContentType(), "audio/midi");
        Check.eq("midi duration", midi.getDuration(), 500000);
        midi.prefetch();
        Check.eq("prefetched", midi.getState(), Player.PREFETCHED);
        Check.yes("unknown control", midi.getControl("ToneControl") == null
                || midi.getControl("ToneControl") instanceof ToneControl);
        VolumeControl vc = (VolumeControl) midi.getControl("VolumeControl");
        Check.yes("volume control", vc != null
                && vc == midi.getControl("javax.microedition.media.control.VolumeControl"));

        final StringBuffer events = new StringBuffer();
        PlayerListener listener = new PlayerListener() {
            public void playerUpdate(Player p, String event, Object data) {
                // Events arrive on their own thread, in order. When the
                // duration becomes known is up to the implementation.
                if (!event.equals(DURATION_UPDATED)) {
                    events.append(' ').append(event);
                }
            }
        };
        midi.addPlayerListener(listener);
        // Five real changes, five VOLUME_CHANGED events.
        Check.eq("volume level", vc.setLevel(40), 40);
        Check.eq("volume clamps to 100", vc.setLevel(150), 100);
        vc.setLevel(40);
        vc.setMute(true);
        Check.yes("mute", vc.isMuted() && vc.getLevel() == 40);
        vc.setMute(false);

        show(screen, "midi, twice");
        midi.setLoopCount(2);
        midi.start();
        Check.eq("started", midi.getState(), Player.STARTED);
        Check.fails("loop count while started", IllegalStateException.class, new Check.Action() {
            public void run() {
                midi.setLoopCount(3);
            }
        });
        Thread.sleep(1300);   // two loops of 500 ms
        Check.eq("prefetched again at the end", midi.getState(), Player.PREFETCHED);
        Check.eq("media time at the end", midi.getMediaTime(), 500000);
        Check.eq("events of two loops", events.toString(),
                " volumeChanged volumeChanged volumeChanged volumeChanged volumeChanged started endOfMedia endOfMedia");
        midi.deallocate();
        Check.eq("deallocate", midi.getState(), Player.REALIZED);
        Thread.sleep(50);
        events.setLength(0);
        midi.close();
        Check.eq("closed", midi.getState(), Player.CLOSED);
        Thread.sleep(50);
        Check.eq("close event", events.toString(), " closed");
        Check.fails("start after close", IllegalStateException.class, new Check.Action() {
            public void run() throws MediaException {
                midi.start();
            }
        });

        // ---- WAV: stop part-way, media time, resume
        show(screen, "wav");
        Player wav = player(wav(4000), "audio/x-wav");
        wav.realize();
        Check.eq("wav duration", wav.getDuration(), 500000);
        events.setLength(0);
        wav.addPlayerListener(listener);
        wav.start();
        Thread.sleep(200);
        wav.stop();
        long at = wav.getMediaTime();
        Check.yes("stopped part-way", wav.getState() == Player.PREFETCHED && at > 0 && at < 500000);
        wav.start();
        Thread.sleep(500);
        Check.eq("wav events", events.toString(), " started stopped started endOfMedia");
        Check.eq("wav media time at the end", wav.getMediaTime(), 500000);
        wav.setMediaTime(0);
        Check.eq("rewound", wav.getMediaTime(), 0);
        wav.close();

        // ---- a tone sequence
        show(screen, "tones");
        Player tones = Manager.createPlayer(Manager.TONE_DEVICE_LOCATOR);
        tones.realize();
        final ToneControl tc = (ToneControl) tones.getControl("ToneControl");
        Check.yes("tone control", tc != null);
        // 120 beats a minute, 64 per whole note: 16 is a quarter note, 500 ms.
        byte[] seq = {
            ToneControl.VERSION, 1, ToneControl.TEMPO, 30, ToneControl.RESOLUTION, 64,
            ToneControl.BLOCK_START, 0, 60, 16, 62, 16, ToneControl.BLOCK_END, 0,
            ToneControl.PLAY_BLOCK, 0, ToneControl.SILENCE, 8, 64, 16};
        tc.setSequence(seq);
        Check.eq("tone sequence duration", tones.getDuration(), 1750000);
        Check.fails("bad tone sequence", IllegalArgumentException.class, new Check.Action() {
            public void run() {
                tc.setSequence(new byte[] {ToneControl.VERSION, 1, 60});
            }
        });
        tones.start();
        Thread.sleep(1900);
        Check.eq("tone sequence played", tones.getState(), Player.PREFETCHED);
        tones.close();

        // ---- Manager.playTone and bad locators
        show(screen, "a tone");
        Manager.playTone(69, 300, 80);   // A4, 440 Hz
        Check.fails("note above 127", IllegalArgumentException.class, new Check.Action() {
            public void run() throws MediaException {
                Manager.playTone(128, 100, 50);
            }
        });
        Check.fails("negative duration", IllegalArgumentException.class, new Check.Action() {
            public void run() throws MediaException {
                Manager.playTone(60, -1, 50);
            }
        });
        Check.fails("unknown locator", MediaException.class, new Check.Action() {
            public void run() throws Exception {
                Manager.createPlayer("nothing://here");
            }
        });
        Thread.sleep(400);
    }
}
