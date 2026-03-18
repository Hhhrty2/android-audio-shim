package de.maxhenkel.shim;

import javax.sound.sampled.*;
import java.util.concurrent.atomic.AtomicBoolean;

public class AndroidTargetDataLine implements TargetDataLine {

    private AudioFormat format;
    private int bufferSizeBytes;
    private final AtomicBoolean open   = new AtomicBoolean(false);
    private final AtomicBoolean active = new AtomicBoolean(false);
    private long framesRead = 0;

    public AndroidTargetDataLine(AudioFormat format) {
        this.format = format;
    }

    @Override
    public void open(AudioFormat format, int bufferSize) throws LineUnavailableException {
        if (open.get()) return;
        if (!NativeAudio.isAvailable())
            throw new LineUnavailableException("libaudioshim.so not loaded: " + NativeAudio.getLoadError());

        this.format = format;
        int rate = (int) format.getSampleRate();
        bufferSizeBytes = (bufferSize > 0) ? bufferSize : rate * 2 / 5;

        int framesPerBuffer = bufferSizeBytes / 2;
        int r = NativeAudio.open(rate, framesPerBuffer);
        if (r != 0)
            throw new LineUnavailableException("AAudio open failed: " + r
                    + ". Check RECORD_AUDIO permission.");

        open.set(true);
        framesRead = 0;
    }

    @Override public void open(AudioFormat fmt) throws LineUnavailableException { open(fmt, 0); }
    @Override public void open()               throws LineUnavailableException { open(format, 0); }

    @Override
    public void start() {
        if (!open.get() || active.get()) return;
        int r = NativeAudio.start();
        if (r != 0) throw new RuntimeException("AAudio start failed: " + r);
        active.set(true);
    }

    @Override
    public void stop() {
        if (!open.get() || !active.get()) return;
        NativeAudio.stop();
        active.set(false);
    }

    @Override
    public void close() {
        if (!open.get()) return;
        if (active.get()) stop();
        NativeAudio.close();
        open.set(false);
    }

    @Override
    public int read(byte[] b, int off, int len) {
        if (!open.get() || !active.get()) return 0;
        int n = NativeAudio.read(b, off, len);
        if (n > 0) framesRead += n / format.getFrameSize();
        return n < 0 ? 0 : n;
    }

    @Override public boolean isOpen()    { return open.get(); }
    @Override public boolean isActive()  { return active.get(); }
    @Override public boolean isRunning() { return active.get(); }
    @Override public void    drain()     {}
    @Override public void    flush()     {}
    @Override public int     available() { return bufferSizeBytes; }
    @Override public int     getBufferSize() { return bufferSizeBytes; }
    @Override public AudioFormat getFormat() { return format; }
    @Override public int  getFramePosition()       { return (int) framesRead; }
    @Override public long getLongFramePosition()   { return framesRead; }
    @Override public long getMicrosecondPosition() {
        return (long)(framesRead * 1_000_000.0 / format.getSampleRate());
    }
    @Override public float getLevel() { return AudioSystem.NOT_SPECIFIED; }
    @Override public Line.Info getLineInfo() {
        return new DataLine.Info(TargetDataLine.class, format);
    }
    @Override public Control[] getControls()                      { return new Control[0]; }
    @Override public boolean   isControlSupported(Control.Type t) { return false; }
    @Override public Control   getControl(Control.Type t) { throw new IllegalArgumentException(); }
    @Override public void addLineListener(LineListener l)    {}
    @Override public void removeLineListener(LineListener l) {}
}
