package java.lang;

/**
 * Basic numeric functions. The trigonometric functions and square root are
 * the virtual machine's natives, computed as java.lang.StrictMath does.
 */
public final class Math {
    public static final double E = 2.718281828459045;
    public static final double PI = 3.141592653589793;

    private static final double DEGREES_TO_RADIANS = 0.017453292519943295;
    private static final double RADIANS_TO_DEGREES = 57.29577951308232;
    private static final long NEGATIVE_ZERO_DOUBLE = 0x8000000000000000L;
    private static final int NEGATIVE_ZERO_FLOAT = 0x80000000;

    private Math() {
    }

    public static native double sin(double a);

    public static native double cos(double a);

    public static native double tan(double a);

    public static native double sqrt(double a);

    public static native double ceil(double a);

    public static native double floor(double a);

    public static double toRadians(double angdeg) {
        return angdeg * DEGREES_TO_RADIANS;
    }

    public static double toDegrees(double angrad) {
        return angrad * RADIANS_TO_DEGREES;
    }

    /** |a|; MIN_VALUE stays negative, as it has no positive int. */
    public static int abs(int a) {
        return a < 0 ? -a : a;
    }

    public static long abs(long a) {
        return a < 0 ? -a : a;
    }

    /* 0 - a turns -0.0 into +0.0, which a < 0 would not catch. */
    public static float abs(float a) {
        return a <= 0.0f ? 0.0f - a : a;
    }

    public static double abs(double a) {
        return a <= 0.0 ? 0.0 - a : a;
    }

    public static int max(int a, int b) {
        return a >= b ? a : b;
    }

    public static long max(long a, long b) {
        return a >= b ? a : b;
    }

    /** The larger; NaN if either is, and +0.0 is larger than -0.0. */
    public static float max(float a, float b) {
        if (a != a) {
            return a;
        }
        if (a == 0.0f && b == 0.0f) {
            return Float.floatToIntBits(a) == NEGATIVE_ZERO_FLOAT ? b : a;
        }
        return a >= b ? a : b;
    }

    public static double max(double a, double b) {
        if (a != a) {
            return a;
        }
        if (a == 0.0 && b == 0.0) {
            return Double.doubleToLongBits(a) == NEGATIVE_ZERO_DOUBLE ? b : a;
        }
        return a >= b ? a : b;
    }

    public static int min(int a, int b) {
        return a <= b ? a : b;
    }

    public static long min(long a, long b) {
        return a <= b ? a : b;
    }

    /** The smaller; NaN if either is, and -0.0 is smaller than +0.0. */
    public static float min(float a, float b) {
        if (a != a) {
            return a;
        }
        if (a == 0.0f && b == 0.0f) {
            return Float.floatToIntBits(a) == NEGATIVE_ZERO_FLOAT ? a : b;
        }
        return a <= b ? a : b;
    }

    public static double min(double a, double b) {
        if (a != a) {
            return a;
        }
        if (a == 0.0 && b == 0.0) {
            return Double.doubleToLongBits(a) == NEGATIVE_ZERO_DOUBLE ? a : b;
        }
        return a <= b ? a : b;
    }
}
