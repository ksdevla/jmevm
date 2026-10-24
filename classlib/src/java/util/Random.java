package java.util;

/**
 * Pseudo-random numbers from the 48-bit linear congruential generator the
 * Java platform specifies, so a seed gives the numbers it gives anywhere.
 */
public class Random {
    private static final long MULTIPLIER = 0x5deece66dL;
    private static final long ADDEND = 0xbL;
    private static final long MASK = (1L << 48) - 1;

    private static long created;   /* sets apart generators made in the same millisecond */

    private long seed;

    /** A generator seeded from the clock. */
    public Random() {
        this(System.currentTimeMillis() ^ (nextSerial() * 0x9e3779b97f4a7c15L));
    }

    public Random(long seed) {
        setSeed(seed);
    }

    private static synchronized long nextSerial() {
        return ++created;
    }

    public synchronized void setSeed(long seed) {
        this.seed = (seed ^ MULTIPLIER) & MASK;
    }

    /** The next bits (1 to 32) random bits. */
    protected synchronized int next(int bits) {
        seed = (seed * MULTIPLIER + ADDEND) & MASK;
        return (int) (seed >>> (48 - bits));
    }

    public int nextInt() {
        return next(32);
    }

    /** A number from 0 up to n, each equally likely. */
    public int nextInt(int n) {
        if (n <= 0) {
            throw new IllegalArgumentException("n must be positive");
        }
        if ((n & -n) == n) {
            return (int) ((n * (long) next(31)) >> 31);   /* a power of two: the high bits */
        }
        int bits;
        int v;
        do {
            /* reject the top values that would make low results likelier */
            bits = next(31);
            v = bits % n;
        } while (bits - v + (n - 1) < 0);
        return v;
    }

    public long nextLong() {
        return ((long) next(32) << 32) + next(32);
    }

    /** A float from 0.0 up to 1.0. */
    public float nextFloat() {
        return next(24) / (float) (1 << 24);
    }

    /** A double from 0.0 up to 1.0. */
    public double nextDouble() {
        return (((long) next(26) << 27) + next(27)) / (double) (1L << 53);
    }
}
