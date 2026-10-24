package jmevm.test.game;

import java.io.IOException;
import java.io.InputStream;

import javax.microedition.io.ConnectionNotFoundException;
import javax.microedition.io.Connector;

/** Resources from the jar (CLDC 1.1) and the Generic Connection Framework. */
final class IoChecks {
    private IoChecks() {
    }

    static String read(InputStream in) throws IOException {
        if (in == null) {
            return null;
        }
        StringBuffer sb = new StringBuffer();
        for (int c; (c = in.read()) >= 0;) {
            sb.append((char) c);
        }
        in.close();
        return sb.toString();
    }

    static void run() throws IOException {
        Check.eq("io resource", read(IoChecks.class.getResourceAsStream("/data.txt")), "jmevm test data\n");
        Check.eq("io resource next to the class", read(IoChecks.class.getResourceAsStream("note.txt")),
                "next to the class");
        Check.eq("io absolute path to the same", read(IoChecks.class.getResourceAsStream("/jmevm/test/game/note.txt")),
                "next to the class");
        Check.yes("io missing resource", IoChecks.class.getResourceAsStream("/missing.txt") == null);
        InputStream in = IoChecks.class.getResourceAsStream("/data.txt");
        byte[] buf = new byte[4];
        Check.yes("io read into an array", in.read(buf, 0, 4) == 4 && buf[0] == 'j' && buf[3] == 'v');
        Check.eq("io skip", in.skip(7), 7);
        Check.eq("io read after skip", in.read(), 'd');
        in.close();
        Check.fails("io unknown protocol", ConnectionNotFoundException.class, new Check.Action() {
            public void run() throws IOException {
                Connector.open("nothing:here");
            }
        });
        Check.fails("io no network", IOException.class, new Check.Action() {
            public void run() throws IOException {
                Connector.open("http://127.0.0.1:1/").close();
            }
        });
        Check.fails("io malformed name", IllegalArgumentException.class, new Check.Action() {
            public void run() throws IOException {
                Connector.open("no colon");
            }
        });
    }
}
