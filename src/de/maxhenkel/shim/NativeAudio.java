package de.maxhenkel.shim;

import java.io.*;

public class NativeAudio {

    private static boolean loaded = false;
    private static String loadError = null;

    static {
        String[] candidates = {
            "/data/data/git.artdeell.mojo/files/libaudioshim.so",
            "/data/data/git.artdeell.mojo/runtimes/Internal/lib/aarch64/server/libaudioshim.so",
            System.getProperty("user.dir") + "/libaudioshim.so",
            System.getProperty("java.io.tmpdir") + "/libaudioshim.so",
        };
        Throwable lastErr = null;
        for (String path : candidates) {
            try {
                File f = new File(path);
                if (!f.exists()) {
                    InputStream is = NativeAudio.class.getResourceAsStream("/natives/libaudioshim.so");
                    if (is == null) { lastErr = new IOException("not found: " + path); continue; }
                    f.getParentFile().mkdirs();
                    FileOutputStream fos = new FileOutputStream(f);
                    byte[] buf = new byte[8192];
                    int n;
                    while ((n = is.read(buf)) != -1) fos.write(buf, 0, n);
                    fos.close();
                    is.close();
                    f.setExecutable(true, false);
                }
                System.load(f.getAbsolutePath());
                loaded = true;
                break;
            } catch (Throwable t) { lastErr = t; }
        }
        if (!loaded && lastErr != null) loadError = lastErr.toString();
    }

    public static boolean isAvailable() { return loaded; }
    public static String getLoadError() { return loadError; }

    public static native int open(int sampleRate, int framesPerBuffer);
    public static native int start();
    public static native int stop();
    public static native int close();
    public static native int read(byte[] buf, int offset, int length);
}
