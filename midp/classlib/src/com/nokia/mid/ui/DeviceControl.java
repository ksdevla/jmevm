package com.nokia.mid.ui;

/** Lights and vibration: there are none here, so nothing happens. */
public class DeviceControl {
    private DeviceControl() {
    }

    public static void setLights(int num, int level) {
        if (level < 0 || level > 100) {
            throw new IllegalArgumentException("level " + level);
        }
    }

    public static void flashLights(long duration) {
        if (duration < 0) {
            throw new IllegalArgumentException("duration " + duration);
        }
    }

    public static void startVibra(int freq, long duration) {
        if (freq < 0 || freq > 100 || duration < 0) {
            throw new IllegalArgumentException("vibra " + freq + " for " + duration);
        }
    }

    public static void stopVibra() {
    }
}
