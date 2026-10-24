package javax.microedition.media.control;

import javax.microedition.media.Control;

/** The playback rate, in thousandths of percent (JSR 135). */
public interface RateControl extends Control {
    int setRate(int millirate);

    int getRate();

    int getMaxRate();

    int getMinRate();
}
