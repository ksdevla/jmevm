package jmevm.test.game;

import javax.microedition.rms.InvalidRecordIDException;
import javax.microedition.rms.RecordComparator;
import javax.microedition.rms.RecordEnumeration;
import javax.microedition.rms.RecordFilter;
import javax.microedition.rms.RecordListener;
import javax.microedition.rms.RecordStore;
import javax.microedition.rms.RecordStoreException;
import javax.microedition.rms.RecordStoreNotFoundException;
import javax.microedition.rms.RecordStoreNotOpenException;

/** Record stores (javax.microedition.rms), against the MIDP 2.0 specification. */
final class RmsChecks {
    private RmsChecks() {
    }

    static byte[] bytes(String s) {
        return s.getBytes();
    }

    static String text(byte[] b) {
        return b == null ? null : new String(b);
    }

    /** Counts this program's runs in a record store of its own: 1 on a fresh data directory. */
    static int countRun() throws RecordStoreException {
        RecordStore rs = RecordStore.openRecordStore("runs", true);
        int run;
        if (rs.getNumRecords() == 0) {
            run = 1;
            rs.addRecord(new byte[] {1}, 0, 1);
        } else {
            run = rs.getRecord(1)[0] + 1;
            rs.setRecord(1, new byte[] {(byte) run}, 0, 1);
        }
        rs.closeRecordStore();
        return run;
    }

    static void run() throws Exception {
        final RecordStore rs = RecordStore.openRecordStore("test", true);
        Check.yes("rms open again gives the same store", RecordStore.openRecordStore("test", true) == rs);
        Check.eq("rms name", rs.getName(), "test");
        Check.eq("rms starts empty", rs.getNumRecords(), 0);
        Check.eq("rms first id", rs.getNextRecordID(), 1);
        Check.yes("rms sizes", rs.getSize() >= 0 && rs.getSizeAvailable() > 0);

        final StringBuffer log = new StringBuffer();
        RecordListener listener = new RecordListener() {
            public void recordAdded(RecordStore s, int id) {
                log.append(" added ").append(id);
            }

            public void recordChanged(RecordStore s, int id) {
                log.append(" changed ").append(id);
            }

            public void recordDeleted(RecordStore s, int id) {
                log.append(" deleted ").append(id);
            }
        };
        rs.addRecordListener(listener);
        int v0 = rs.getVersion();
        Check.eq("rms ids count up", rs.addRecord(bytes("c"), 0, 1) * 100 + rs.addRecord(bytes("xaa"), 1, 2) * 10
                + rs.addRecord(bytes("b"), 0, 1), 123);
        Check.eq("rms empty record", rs.addRecord(null, 0, 0), 4);
        Check.eq("rms count", rs.getNumRecords(), 4);
        Check.eq("rms next id", rs.getNextRecordID(), 5);
        Check.eq("rms version counts changes", rs.getVersion() - v0, 4);
        Check.eq("rms getRecord", text(rs.getRecord(2)), "aa");
        Check.eq("rms empty record reads as null", text(rs.getRecord(4)), null);
        Check.eq("rms record sizes", rs.getRecordSize(2) * 10 + rs.getRecordSize(4), 20);
        byte[] buf = new byte[4];
        Check.eq("rms getRecord into a buffer", rs.getRecord(2, buf, 1), 2);
        Check.yes("rms buffer contents", buf[0] == 0 && buf[1] == 'a' && buf[2] == 'a' && buf[3] == 0);
        Check.fails("rms buffer too small", ArrayIndexOutOfBoundsException.class, new Check.Action() {
            public void run() throws Exception {
                rs.getRecord(2, new byte[1], 0);
            }
        });
        rs.setRecord(3, bytes("bb"), 0, 2);
        Check.eq("rms setRecord", text(rs.getRecord(3)), "bb");
        rs.deleteRecord(1);
        Check.fails("rms deleted record", InvalidRecordIDException.class, new Check.Action() {
            public void run() throws Exception {
                rs.getRecord(1);
            }
        });
        Check.fails("rms delete twice", InvalidRecordIDException.class, new Check.Action() {
            public void run() throws Exception {
                rs.deleteRecord(1);
            }
        });
        Check.fails("rms record 0", InvalidRecordIDException.class, new Check.Action() {
            public void run() throws Exception {
                rs.getRecordSize(0);
            }
        });
        Check.eq("rms ids are not reused", rs.addRecord(bytes("ab"), 0, 2), 5);
        Check.eq("rms listener", log.toString(), " added 1 added 2 added 3 added 4 changed 3 deleted 1 added 5");
        rs.removeRecordListener(listener);
        rs.setRecord(5, bytes("ab"), 0, 2);
        Check.eq("rms listener removed", log.toString().endsWith("added 5") ? 1 : 0, 1);

        // Records now: 2 "aa", 3 "bb", 4 empty, 5 "ab".
        RecordFilter notEmpty = new RecordFilter() {
            public boolean matches(byte[] r) {
                return r != null && r.length > 0;
            }
        };
        RecordComparator byText = new RecordComparator() {
            public int compare(byte[] a, byte[] b) {
                int c = (a == null ? "" : new String(a)).compareTo(b == null ? "" : new String(b));
                return c < 0 ? PRECEDES : c > 0 ? FOLLOWS : EQUIVALENT;
            }
        };
        final RecordEnumeration e = rs.enumerateRecords(notEmpty, byText, false);
        Check.eq("rms enumeration count", e.numRecords(), 3);
        Check.eq("rms enumeration order", e.nextRecordId() * 100 + e.nextRecordId() * 10 + e.nextRecordId(), 253);
        Check.yes("rms enumeration end", !e.hasNextElement());
        Check.fails("rms past the end", InvalidRecordIDException.class, new Check.Action() {
            public void run() throws Exception {
                e.nextRecordId();
            }
        });
        e.reset();
        Check.eq("rms enumeration reset", text(e.nextRecord()), "aa");
        rs.addRecord(bytes("a"), 0, 1);   // id 6
        Check.eq("rms enumeration not kept updated", e.numRecords(), 3);
        e.rebuild();
        Check.eq("rms enumeration rebuilt", e.numRecords(), 4);
        e.destroy();
        Check.fails("rms destroyed enumeration", IllegalStateException.class, new Check.Action() {
            public void run() throws Exception {
                e.numRecords();
            }
        });
        RecordEnumeration live = rs.enumerateRecords(null, byText, true);
        Check.eq("rms all records", live.numRecords(), 5);
        rs.deleteRecord(4);
        Check.eq("rms kept updated", live.numRecords(), 4);
        Check.eq("rms kept updated order", live.nextRecordId() * 10 + live.nextRecordId(), 62);
        live.destroy();
        RecordEnumeration all = rs.enumerateRecords(null, null, false);
        int sum = 0;
        while (all.hasNextElement()) {
            sum += all.nextRecordId();
        }
        Check.eq("rms unordered enumeration sees every record", sum, 2 + 3 + 5 + 6);

        Check.fails("rms delete an open store", RecordStoreException.class, new Check.Action() {
            public void run() throws Exception {
                RecordStore.deleteRecordStore("test");
            }
        });
        int version = rs.getVersion();
        rs.closeRecordStore();
        Check.eq("rms still open after one of two closes", rs.getNumRecords(), 4);
        rs.closeRecordStore();
        Check.fails("rms closed", RecordStoreNotOpenException.class, new Check.Action() {
            public void run() throws Exception {
                rs.getNumRecords();
            }
        });

        RecordStore again = RecordStore.openRecordStore("test", false);
        Check.yes("rms reopened", again.getNumRecords() == 4 && again.getNextRecordID() == 7
                && again.getVersion() == version && "bb".equals(text(again.getRecord(3))));
        again.closeRecordStore();
        Check.fails("rms missing store", RecordStoreNotFoundException.class, new Check.Action() {
            public void run() throws Exception {
                RecordStore.openRecordStore("missing", false);
            }
        });
        Check.fails("rms name too long", IllegalArgumentException.class, new Check.Action() {
            public void run() throws Exception {
                RecordStore.openRecordStore("123456789012345678901234567890123", true);
            }
        });
        Check.fails("rms empty name", IllegalArgumentException.class, new Check.Action() {
            public void run() throws Exception {
                RecordStore.openRecordStore("", true);
            }
        });
        RecordStore shared = RecordStore.openRecordStore("test", "jmevm", "Game");
        Check.eq("rms own suite by vendor and name", shared.getNumRecords(), 4);
        shared.closeRecordStore();
        Check.fails("rms another suite's missing store", RecordStoreNotFoundException.class, new Check.Action() {
            public void run() throws Exception {
                RecordStore.openRecordStore("test", "someone", "Else");
            }
        });
        Check.yes("rms list", contains(RecordStore.listRecordStores(), "test")
                && contains(RecordStore.listRecordStores(), "runs"));
        RecordStore.deleteRecordStore("test");
        Check.yes("rms deleted store", !contains(RecordStore.listRecordStores(), "test"));
    }

    static boolean contains(String[] names, String name) {
        for (int i = 0; names != null && i < names.length; i++) {
            if (names[i].equals(name)) {
                return true;
            }
        }
        return false;
    }
}
