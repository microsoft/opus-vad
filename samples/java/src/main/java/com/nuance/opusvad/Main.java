package com.nuance.samples;

import com.nuance.opusvad.jni.OpusVAD;
import com.nuance.opusvad.jni.OpusVAD.Listener;
import com.nuance.opusvad.jni.OpusVADOptions;
import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.util.UUID;
import java.util.logging.Level;
import java.util.logging.Logger;
import org.apache.commons.cli.*;

public class Main {

    private static final OpusVAD.Listener mVADListener = new OpusVAD.Listener() {
                @Override
                public void onStartOfSpeech(String ctx, int pos) {
                    System.out.println("[" + ctx + "] OPUSVAD_SOS pos: " + pos);
                }
                @Override
                public void onEndOfSpeech(String ctx, int pos) {
                    System.out.println("[" + ctx + "] OPUSVAD_EOS pos: " + pos);
                }

            };    

    public static Options initializeCLI() {
        Options options = new Options();

        options.addOption(Option.builder("h")
            .argName("help")
            .required(false)
            .longOpt("help")
            .desc("Display help")
            .build());

        options.addOption(Option.builder("f")
            .argName("file")
            .longOpt("file")
            .hasArg()
            .desc("Audio file to process")
            .build());

        options.addOption(Option.builder("adpcm")
            .argName("adpcm")
            .required(false)
            .longOpt("adpcm")
            .desc("Specify if the input audio file is adpcm encoded")
            .build());

        options.addOption(Option.builder("hn")
            .argName("high-nibble")
            .required(false)
            .longOpt("high-nibble")
            .desc("If passing in adpcm encoded audio, specify if it is high-nibble ordered")
            .build());

        options.addOption(Option.builder("sos")
            .argName("sos")
            .hasArg()
            .required(false)
            .longOpt("sos")
            .desc("Start of speech window in ms. Default: " + OpusVAD.DEFAULT_SOS_WINDOW_MS)
            .build());

        options.addOption(Option.builder("eos")
            .argName("eos")
            .hasArg()
            .required(false)
            .longOpt("eos")
            .desc("End of speech window in ms. Default: " + OpusVAD.DEFAULT_EOS_WINDOW_MS)
            .build());

        options.addOption(Option.builder("s")
            .argName("sensitivity")
            .hasArg()
            .required(false)
            .longOpt("sensitivity")
            .desc("Speech detection sensitivity in the range of 0 to 100. Default: " + OpusVAD.DEFAULT_SPEECH_DETECTION_SENSITIVITY)
            .build());
        
        options.addOption(Option.builder("c")
            .argName("complexity")
            .hasArg()
            .required(false)
            .longOpt("complexity")
            .desc("Opus VAD complexity setting in the range of 0 to 10. Default: " + OpusVAD.DEFAULT_COMPLEXITY)
            .build());

        options.addOption(Option.builder("brt")
            .argName("bit_rate_type")
            .hasArg()
            .required(false)
            .longOpt("bit_rate_type")
            .desc("Opus VAD bit rate type. Options are 0 (VBR), 1 (CVBR), 2 (CBR). Default: " + OpusVAD.DEFAULT_BIT_RATE_TYPE)
            .build());
        
        return options;
    }

    public static void printUsage(Options options) {
        HelpFormatter formatter = new HelpFormatter();
        formatter.setOptionComparator(null);
        formatter.setWidth(800);
        
        String path = Main.class.getProtectionDomain().getCodeSource().getLocation().getFile();
        File f = new File(path);
        String jar = f.getName();

        formatter.printHelp("java -jar " + jar, options);
    }

    public static void main(String[] args) {

        try {
            CommandLine cli = new DefaultParser().parse(Main.initializeCLI(), args);

            if (cli.hasOption("help")) {
                Main.printUsage(Main.initializeCLI());
                System.exit(0);
            }

            if (!cli.hasOption("file")) {
                throw new org.apache.commons.cli.MissingOptionException("Missing required option: f");
            }
            
          	boolean useAdpcm = cli.hasOption("adpcm");
            int high_nibble_first = cli.hasOption("high-nibble") ? 1 : 0;
            int bitRateType = cli.hasOption("bit_rate_type") ? Integer.parseInt(cli.getOptionValue("bit_rate_type")) : OpusVAD.DEFAULT_BIT_RATE_TYPE;
            int complexity = cli.hasOption("complexity") ? Integer.parseInt(cli.getOptionValue("complexity")) : OpusVAD.DEFAULT_COMPLEXITY;
            int sensitivity = cli.hasOption("sensitivity") ? Integer.parseInt(cli.getOptionValue("sensitivity")) : OpusVAD.DEFAULT_SPEECH_DETECTION_SENSITIVITY;
            int sos = cli.hasOption("sos") ? Integer.parseInt(cli.getOptionValue("sos")) : OpusVAD.DEFAULT_SOS_WINDOW_MS;
            int eos = cli.hasOption("eos") ? Integer.parseInt(cli.getOptionValue("eos")) : OpusVAD.DEFAULT_EOS_WINDOW_MS;
                   
            UUID uuid = UUID.randomUUID();

            OpusVADOptions options = new OpusVADOptions(uuid.toString(), complexity, bitRateType, sos, eos, sensitivity);
            OpusVAD vad = new OpusVAD(useAdpcm, options, mVADListener);

            System.out.println("Frame bytes: " + vad.getFrameBytes());
            System.out.println("Buffer size (bytes): " + (vad.getMaxBufferSamples() * OpusVAD.SAMPLE_BYTES));

            if( useAdpcm ) {
                if (high_nibble_first == 1) {
                    vad.setHighNibbleFirst();
                } else {
                    vad.setLowNibbleFirst();
                }
            }

            BufferedInputStream is = new BufferedInputStream(new FileInputStream(cli.getOptionValue("file")));
            byte[] curframe = new byte[vad.getFrameBytes()];

            int curframepos = 0;
            int read;

            while ((read = is.read(curframe, curframepos, curframe.length - curframepos)) >= 0) {
                curframepos += read;
                if (curframepos == curframe.length) {
                  vad.processAudioByteArray(curframe, 0, curframepos);
                  curframepos = 0;
                }
            }
            if (curframepos > 0) {
                vad.processAudioByteArray(curframe, 0, curframepos);
            }
            is.close();
        } catch (Exception ex) {
            Logger.getLogger(Main.class.getName()).log(Level.SEVERE, null, ex);
        }
    }
}
