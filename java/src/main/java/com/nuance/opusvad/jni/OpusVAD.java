package com.nuance.opusvad.jni;

/* IMPORTANT NOTE:
 * The native functions in this class will expect the functions
 * prefixed with "Java_com_nuance_opusvad_jni_OpusVAD_".
 * If you move this class to another package, you much change
 * the function signatures in the native library.
 */

import java.nio.ByteBuffer;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;

public class OpusVAD {
    public static final int OPUSVAD_BIT_RATE_TYPE_VBR = 0;
    public static final int OPUSVAD_BIT_RATE_TYPE_CVBR = 1;
    public static final int OPUSVAD_BIT_RATE_TYPE_CBR = 2;

    public static final int DEFAULT_COMPLEXITY = 3;
    public static final int DEFAULT_BIT_RATE_TYPE = OPUSVAD_BIT_RATE_TYPE_CVBR;
    public static final int DEFAULT_SOS_WINDOW_MS = 220;
    public static final int DEFAULT_EOS_WINDOW_MS = 900;
    public static final int DEFAULT_SPEECH_DETECTION_SENSITIVITY = 20;

    public static final int SAMPLE_BYTES = 2;
    public static final int AUDIO_FREQ = 16000;

    ByteBuffer ctx = null; // Will be modified by native init()
    final ByteBuffer audioBuffer;
    final ByteBuffer encodedBuffer;
    final int frame_size;
    boolean useAdpcm = false;
    int high_nibble_first = 0;

    public interface Listener {
        void onStartOfSpeech(String ctx, int pos);
        void onEndOfSpeech(String ctx, int pos);
    };
    private Listener mVADListener = null;
    
    native final int init(OpusVADOptions options);
    public native final int processAudio(ByteBuffer audiobuf, int num_samples);
    public native final int processAudioAdpcm(ByteBuffer audiobuf, int num_samples, int high_nibble_first);
    native final int getFrameSize();
    public native final int getMaxBufferSamples();
    native final int getOpusEncoded(ByteBuffer outbuf);
    
    void onStartOfSpeech(String ctx, int pos)
    {
	    if (this.mVADListener != null)
	        this.mVADListener.onStartOfSpeech(ctx, pos);
    }
    
    void onEndOfSpeech(String ctx, int pos)
    {
	if (this.mVADListener != null)
	    this.mVADListener.onEndOfSpeech(ctx, pos);
    }

    public static String determinePlatformPath() {
        String os = System.getProperty("os.name").toLowerCase();
        String arch = System.getProperty("os.arch").toLowerCase();

    

        if (os.contains("mac")) {
            return "dist/mac/";
        } else if (os.contains("win")) {
            return "dist/windows/";
        } else if (os.contains("linux")) {
            // Differentiating between Ubuntu and CentOS
            try {
                String content = new String(Files.readAllBytes(Paths.get("/etc/os-release")), StandardCharsets.UTF_8);

                if (content.contains("ID=ubuntu")) {
                    return "dist/ubuntu/";
                } else if (content.contains("ID=centos")) {
                    return "dist/centos/";
                } else {
                    // Fallback for other Linux distributions, adjust as needed
                    return "dist/ubuntu/";
                }
            } catch (IOException e) {
                // Handle the error, for now, default to a generic Linux path
                return "dist/ubuntu/";
            }
        }

        throw new UnsupportedOperationException("Platform not supported: " + os + " " + arch);
    }

    public static void loadLibrary(String libraryName) {
        String platformPath = determinePlatformPath();
        String fullPath = platformPath + System.mapLibraryName(libraryName);

        // Extract and load the library
        try (InputStream in = OpusVAD.class.getClassLoader().getResourceAsStream(fullPath)) {
            File tempLib = File.createTempFile(libraryName, ".tmp");
            Files.copy(in, tempLib.toPath(), StandardCopyOption.REPLACE_EXISTING);
            System.load(tempLib.getAbsolutePath());
            tempLib.deleteOnExit();
        } catch (IOException e) {
            throw new RuntimeException("Failed to load library " + libraryName, e);
        }

    }

    public OpusVAD(boolean useAdpcm, OpusVADOptions options, OpusVAD.Listener listener) throws Exception {

        loadLibrary("opus");
        loadLibrary("opusvadjava");

        int res = init(options);
        if (res != 0) {
            throw new Exception("Error with Init: " + res);
        }
        this.useAdpcm = useAdpcm;
	    this.mVADListener = listener;
	
        frame_size = getFrameSize();
        audioBuffer = (useAdpcm) ? ByteBuffer.allocateDirect(frame_size / 2) : ByteBuffer.allocateDirect(frame_size * SAMPLE_BYTES);
        encodedBuffer = (useAdpcm) ? ByteBuffer.allocateDirect(frame_size / 2) : ByteBuffer.allocateDirect(frame_size * SAMPLE_BYTES);
    }

    public void setHighNibbleFirst() {
        high_nibble_first = 1;
    }

    public void setLowNibbleFirst() {
      high_nibble_first = 0;
    }

    public int processAudioByteArray(byte[] audiobuf, int offset, int len)
    {
        audioBuffer.position(0);
        audioBuffer.put(audiobuf, offset, len);

        return ( (useAdpcm) ? processAudioAdpcm(audioBuffer, len * 2, high_nibble_first) : processAudio(audioBuffer, len / SAMPLE_BYTES) );
    }

    public int getFrameSamples()
    {
        return frame_size;
    }

    public int getFrameBytes() {
          return ( (useAdpcm) ? getFrameSamples() / 2 : getFrameSamples() * SAMPLE_BYTES );
    }

    public int getOpusEncodedBytes(byte[] out, int pos, int maxlen) {
       encodedBuffer.position(0);
       int res = getOpusEncoded(encodedBuffer);
       if (res >= 0) {
           if (maxlen < res) {
               return -1;
           }
           encodedBuffer.get(out, pos, res);
       }
       return res;
    }
}
