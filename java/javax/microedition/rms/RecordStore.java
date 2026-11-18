package javax.microedition.rms;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.util.ArrayList;

/** Partidas guardadas: un fichero por almacen en rms/, reescrito entero en cada cambio. */
public class RecordStore {
    private static final String DIR = "rms";
    private final String nombre;
    private final ArrayList<byte[]> regs = new ArrayList<byte[]>();   // registro i+1; null = borrado

    private RecordStore(String nombre) { this.nombre = nombre; }

    private static File fichero(String nombre) {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < nombre.length(); i++) {
            char c = nombre.charAt(i);
            if (c >= '0' && c <= '9' || c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z') {
                sb.append(c);
            } else {
                sb.append('_').append((int) c).append('_');
            }
        }
        return new File(DIR, sb.toString() + ".rms");
    }

    public static RecordStore openRecordStore(String nombre, boolean crear) throws RecordStoreException {
        RecordStore rs = new RecordStore(nombre);
        File f = fichero(nombre);
        if (!f.exists()) {
            if (!crear) throw new RecordStoreNotFoundException(nombre);
            rs.guarda();
            return rs;
        }
        try {
            DataInputStream in = new DataInputStream(new FileInputStream(f));
            int n = in.readInt();
            for (int i = 0; i < n; i++) {
                int len = in.readInt();
                if (len < 0) {
                    rs.regs.add(null);
                } else {
                    byte[] b = new byte[len];
                    in.readFully(b);
                    rs.regs.add(b);
                }
            }
            in.close();
        } catch (Exception e) {
            throw new RecordStoreException("lectura: " + e);
        }
        return rs;
    }

    private void guarda() throws RecordStoreException {
        try {
            new File(DIR).mkdirs();
            DataOutputStream out = new DataOutputStream(new FileOutputStream(fichero(nombre)));
            out.writeInt(regs.size());
            for (int i = 0; i < regs.size(); i++) {
                byte[] b = regs.get(i);
                if (b == null) {
                    out.writeInt(-1);
                } else {
                    out.writeInt(b.length);
                    out.write(b);
                }
            }
            out.close();
        } catch (Exception e) {
            throw new RecordStoreException("escritura: " + e);
        }
    }

    private byte[] reg(int id) throws InvalidRecordIDException {
        if (id < 1 || id > regs.size() || regs.get(id - 1) == null) throw new InvalidRecordIDException("id " + id);
        return regs.get(id - 1);
    }

    public static void deleteRecordStore(String nombre) throws RecordStoreException {
        File f = fichero(nombre);
        if (!f.exists()) throw new RecordStoreNotFoundException(nombre);
        f.delete();
    }

    public static String[] listRecordStores() {
        String[] l = new File(DIR).list();
        return l == null || l.length == 0 ? null : l;
    }

    public void closeRecordStore() {}

    public int getNumRecords() {
        int n = 0;
        for (int i = 0; i < regs.size(); i++) if (regs.get(i) != null) n++;
        return n;
    }

    public int addRecord(byte[] data, int off, int len) throws RecordStoreException {
        byte[] b = new byte[len];
        if (len > 0) System.arraycopy(data, off, b, 0, len);
        regs.add(b);
        guarda();
        return regs.size();
    }

    public void setRecord(int id, byte[] data, int off, int len) throws RecordStoreException {
        reg(id);
        byte[] b = new byte[len];
        if (len > 0) System.arraycopy(data, off, b, 0, len);
        regs.set(id - 1, b);
        guarda();
    }

    public byte[] getRecord(int id) throws RecordStoreException {
        byte[] b = reg(id);
        byte[] c = new byte[b.length];
        System.arraycopy(b, 0, c, 0, b.length);
        return c;
    }

    public int getRecord(int id, byte[] buf, int off) throws RecordStoreException {
        byte[] b = reg(id);
        System.arraycopy(b, 0, buf, off, b.length);
        return b.length;
    }

    public int getRecordSize(int id) throws RecordStoreException { return reg(id).length; }
}
