import java.util.Calendar;
import java.util.Date;
import java.util.EmptyStackException;
import java.util.Enumeration;
import java.util.Hashtable;
import java.util.NoSuchElementException;
import java.util.Random;
import java.util.Stack;
import java.util.TimeZone;
import java.util.Vector;

/*
 * java.util: Vector, Stack, Hashtable, Random, Calendar, Date and TimeZone.
 * The expected output is what a desktop JVM prints.
 */
public class TestUtil {
    public static void main(String[] args) {
        vectors();
        stacks();
        hashtables();
        random();
        calendars();
        System.out.println("done");
    }

    static void vectors() {
        Vector v = new Vector(2, 3);
        for (int i = 0; i < 6; i++) {
            v.addElement(new Integer(i * i));
        }
        System.out.println(v + " size " + v.size() + " capacity " + v.capacity());
        v.insertElementAt("first", 0);
        v.insertElementAt(null, 3);
        v.setElementAt("x", 5);
        v.removeElementAt(1);
        System.out.println(v + " " + v.firstElement() + " " + v.lastElement() + " " + v.elementAt(2));
        System.out.println("find " + v.indexOf(null) + " " + v.indexOf(new Integer(25)) + " " + v.indexOf("x", 5) + " "
                + v.lastIndexOf(new Integer(1)) + " " + v.lastIndexOf("first", 3) + " " + v.contains(new Integer(16))
                + " " + v.contains("y"));
        System.out.println("remove " + v.removeElement(new Integer(16)) + " " + v.removeElement("zz") + " " + v);
        v.setSize(7);
        System.out.println("grown " + v + " " + v.size());
        v.setSize(2);
        Object[] copy = new Object[2];
        v.copyInto(copy);
        v.trimToSize();
        System.out.println("shrunk " + v + " " + copy[0] + " " + v.capacity());
        StringBuffer seen = new StringBuffer();
        for (Enumeration e = v.elements(); e.hasMoreElements();) {
            seen.append(e.nextElement()).append(';');
        }
        System.out.println("elements " + seen);
        Enumeration e = v.elements();
        v.removeAllElements();
        System.out.println("empty " + v.isEmpty() + " " + v + " " + e.hasMoreElements());
        try {
            e.nextElement();
        } catch (NoSuchElementException x) {
            System.out.println("no element");
        }
        String[] bad = {"elementAt", "firstElement", "insertElementAt", "removeElementAt", "lastIndexOf"};
        for (int i = 0; i < bad.length; i++) {
            try {
                switch (i) {
                    case 0:
                        v.elementAt(0);
                        break;
                    case 1:
                        v.firstElement();
                        break;
                    case 2:
                        v.insertElementAt("z", 1);
                        break;
                    case 3:
                        v.removeElementAt(-1);
                        break;
                    default:
                        v.lastIndexOf("z", 0);
                        break;
                }
                System.out.println(bad[i] + " passed");
            } catch (RuntimeException x) {
                System.out.println(bad[i] + " " + x.getClass().getName());
            }
        }
        Vector self = new Vector();
        self.addElement(self);
        System.out.println("self " + self);
    }

    static void stacks() {
        Stack s = new Stack();
        System.out.println("push " + s.push("a") + s.push("b") + s.push("c") + " " + s.empty());
        System.out.println("search " + s.search("c") + " " + s.search("a") + " " + s.search("z"));
        System.out.println("pop " + s.pop() + s.pop() + " peek " + s.peek() + " " + s.size());
        s.pop();
        try {
            s.pop();
        } catch (EmptyStackException e) {
            System.out.println("empty stack " + e.getMessage());
        }
    }

    static void hashtables() {
        Hashtable h = new Hashtable();
        String[] words = {"one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten", "eleven"};
        for (int i = 0; i < words.length; i++) {
            h.put(words[i], new Integer(i + 1));
        }
        for (int i = 0; i < 30; i++) {
            h.put(new Integer(i * 7), "n" + i);   /* grows the table twice */
        }
        System.out.println(h.size() + " " + h);
        System.out.println("get " + h.get("seven") + " " + h.get(new Integer(14)) + " " + h.get("zero") + " "
                + h.containsKey("ten") + " " + h.contains("n3") + " " + h.contains("n99"));
        System.out.println("put again " + h.put("one", "uno") + " remove " + h.remove("two") + " " + h.remove("two")
                + " " + h.size());
        StringBuffer keys = new StringBuffer();
        int sum = 0;
        for (Enumeration e = h.keys(); e.hasMoreElements();) {
            Object k = e.nextElement();
            if (k instanceof String) {
                keys.append(k).append(' ');
                h.remove(k);   /* the enumeration does not mind */
            }
        }
        for (Enumeration e = h.elements(); e.hasMoreElements();) {
            sum += ((String) e.nextElement()).length();
        }
        System.out.println("keys " + keys + "values " + sum + " left " + h.size());
        Hashtable small = new Hashtable(0);
        small.put(new Long(5), "five");
        small.put(new Character('q'), Boolean.TRUE);
        small.put("selfish", small);
        System.out.println("small " + small + " " + small.isEmpty());
        small.clear();
        System.out.println("cleared " + small + " " + small.isEmpty());
        try {
            small.put("k", null);
        } catch (NullPointerException e) {
            System.out.println("null value");
        }
        try {
            small.get(null);
        } catch (NullPointerException e) {
            System.out.println("null key");
        }
        try {
            new Hashtable(-1);
        } catch (IllegalArgumentException e) {
            System.out.println("negative capacity");
        }
    }

    static void random() {
        Random r = new Random(42);
        StringBuffer sb = new StringBuffer();
        for (int i = 0; i < 4; i++) {
            sb.append(r.nextInt()).append(' ');
        }
        for (int i = 0; i < 4; i++) {
            sb.append(r.nextInt(10)).append(' ').append(r.nextInt(1 << 20)).append(' ').append(r.nextInt(1000000007))
                    .append(' ');
        }
        System.out.println("ints " + sb);
        System.out.println("others " + r.nextLong() + " " + r.nextFloat() + " " + r.nextDouble());
        r.setSeed(-1);
        int[] counts = new int[6];
        for (int i = 0; i < 6000; i++) {
            counts[r.nextInt(6)]++;
        }
        System.out.println("dice " + counts[0] + " " + counts[1] + " " + counts[2] + " " + counts[3] + " " + counts[4]
                + " " + counts[5]);
        try {
            r.nextInt(0);
        } catch (IllegalArgumentException e) {
            System.out.println("bound must be positive");
        }
        Random a = new Random();
        Random b = new Random();
        System.out.println("unseeded differ " + (a.nextLong() != b.nextLong()));
    }

    static String show(Calendar c) {
        return c.get(Calendar.YEAR) + "-" + (c.get(Calendar.MONTH) + 1) + "-" + c.get(Calendar.DAY_OF_MONTH) + " "
                + c.get(Calendar.HOUR_OF_DAY) + ":" + c.get(Calendar.MINUTE) + ":" + c.get(Calendar.SECOND) + "."
                + c.get(Calendar.MILLISECOND) + " dow " + c.get(Calendar.DAY_OF_WEEK) + " " + c.get(Calendar.HOUR)
                + (c.get(Calendar.AM_PM) == Calendar.AM ? "am" : "pm");
    }

    static void calendars() {
        TimeZone gmt = TimeZone.getTimeZone("GMT");
        Calendar c = Calendar.getInstance(gmt);
        long[] moments = {0L, 951782400000L, 1709210096789L, -1L, -2208988800000L, 4102444800000L, 253402300799999L};
        for (int i = 0; i < moments.length; i++) {
            c.setTime(new Date(moments[i]));
            System.out.println(moments[i] + " " + show(c));
        }
        c.setTime(new Date(0));
        c.set(Calendar.YEAR, 2024);
        c.set(Calendar.MONTH, Calendar.FEBRUARY);
        c.set(Calendar.DATE, 29);
        c.set(Calendar.HOUR_OF_DAY, 23);
        c.set(Calendar.MINUTE, 59);
        System.out.println("set " + c.getTime().getTime() + " " + show(c));
        c.set(Calendar.DATE, 31);   /* February 31st is March 2nd */
        System.out.println("lenient " + show(c));
        c.set(Calendar.MONTH, 13);
        c.set(Calendar.SECOND, -1);
        System.out.println("carry " + show(c));
        c.set(Calendar.HOUR, 3);
        c.set(Calendar.AM_PM, Calendar.PM);
        System.out.println("12-hour " + show(c));
        c.set(Calendar.HOUR_OF_DAY, 1);
        System.out.println("24-hour " + show(c));

        Calendar d = Calendar.getInstance(TimeZone.getTimeZone("GMT+05:30"));
        d.setTime(new Date(0));
        System.out.println("zone " + d.getTimeZone().getID() + " " + d.getTimeZone().getRawOffset() + " " + show(d));
        d.setTimeZone(TimeZone.getTimeZone("GMT-0800"));
        System.out.println("zone " + d.getTimeZone().getID() + " " + show(d) + " " + d.getTime().getTime());
        String[] ids = {"GMT+1", "GMT-11:00", "GMT+2400", "UTC", "GMT+5:7", "Mars/Olympus"};
        for (int i = 0; i < ids.length; i++) {
            TimeZone z = TimeZone.getTimeZone(ids[i]);
            System.out.println("id " + ids[i] + " -> " + z.getID() + " " + z.getRawOffset() + " " + z.useDaylightTime()
                    + " " + z.getOffset(1, 2000, 0, 1, Calendar.SATURDAY, 0));
        }
        Calendar e = Calendar.getInstance(gmt);
        e.setTime(new Date(1000));
        Calendar f = Calendar.getInstance(gmt);
        f.setTime(new Date(2000));
        System.out.println("order " + e.before(f) + " " + e.after(f) + " " + e.equals(f) + " " + e.before("x"));
        f.setTime(new Date(1000));
        System.out.println("equal " + e.equals(f) + " " + new Date(5).equals(new Date(5)) + " "
                + new Date(1L << 33).hashCode());
        Date now = new Date();
        long before = System.currentTimeMillis();
        System.out.println("now " + (Math.abs(now.getTime() - before) < 60000));
    }
}
