using System;
using System.IO;
using System.Collections.Generic;
using System.Runtime.InteropServices;


namespace OpusVADToolDotNet
{	
    public class OpusVAD
    {
        // Define constants
        public const int OPUSVAD_OK = 0;
        public const int OPUSVAD_ALLOC_FAILED = -1;
        public const int OPUSVAD_OPUS_ERROR = -2;
        public const int OPUSVAD_TOO_LARGE = -3;
        public const int OPUSVAD_TOO_SMALL = -4;
        public const int OPUSVAD_PARAM_ERROR = -5;

        // Define bitrate types
        public const int OPUSVAD_BIT_RATE_TYPE_VBR = 0;
        public const int OPUSVAD_BIT_RATE_TYPE_CVBR = 1;
        public const int OPUSVAD_BIT_RATE_TYPE_CBR = 2;

        // OpusVAD options struct
        [StructLayout(LayoutKind.Sequential)]
        public struct OpusVADOptions
        {
            public IntPtr ctx;
            public int complexity;
            public int bitRateType;
            public int sos;
            public int eos;
            public int speechDetectionSensitivity;
            public IntPtr onSOS;
            public IntPtr onEOS;
        }

        // Define OpusVAD functions for P/Invoke
        [DllImport("libopusvad", CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr opusvad_create(out int error, ref OpusVADOptions options);

        [DllImport("libopusvad", CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr opusvad_create_opt(out int error, ref OpusVADOptions options, int frameDur);

        [DllImport("libopusvad", CallingConvention = CallingConvention.Cdecl)]
        public static extern int opusvad_get_frame_size(IntPtr vad);

        [DllImport("libopusvad", CallingConvention = CallingConvention.Cdecl)]
        public static extern int opusvad_get_max_buffer_samples(IntPtr vad);

        [DllImport("libopusvad", CallingConvention = CallingConvention.Cdecl)]
        public static extern int opusvad_process_audio(IntPtr vad, byte[] pcm, int numSamples);

        [DllImport("libopusvad", CallingConvention = CallingConvention.Cdecl)]
        public static extern int opusvad_get_vad_result(IntPtr vad);

        [DllImport("libopusvad", CallingConvention = CallingConvention.Cdecl)]
        public static extern int opusvad_get_opus_encoded(IntPtr vad, IntPtr buffer, int bufferSize);

        [DllImport("libopusvad", CallingConvention = CallingConvention.Cdecl)]
        public static extern void opusvad_destroy(IntPtr vad);

        // Define callback delegate types
        public delegate void OpusVadCallback(IntPtr ptr, uint pos);
    }

	class Program : EventArgs
	{
		const int mFRAME_SIZE_MS = 20;
		const int mLEADING_SILENCE_WINDOW_MS = 120;
		const int mSOS_WINDOW_MS = 220;
		const int mEOS_WINDOW_MS = 500;
		const int mDEFAULT_COMPLEXITY = 3;
		const int mSENSITIVITY = 20;
		const int mAUDIO_BUFFER_SIZE = 50 * 2; // 50 frames a second * 2 seconds should be enough time to setup the ASR connection
		static IntPtr opusvad;
        private static OpusVAD.OpusVadCallback? _vadSOS;
        private static OpusVAD.OpusVadCallback? _vadEOS;
		static Queue<byte[]>? mAudioBuffer;
		static bool connectionReady = true; // Just faking it here
		static bool canStreamAudio = true;  // also a fake
		static bool sosDetected;
		static bool eosDetected;
		static uint sosPosition;
		static uint eosPosition;
		static BinaryWriter? binWriter;

		static void onStartOfSpeech(IntPtr p, uint pos)
		{
			Console.WriteLine("OpusVad Start SpeechEvent {0:D}ms", pos - mSOS_WINDOW_MS - mLEADING_SILENCE_WINDOW_MS);
			sosDetected = true;
			sosPosition = pos;
		}
		static void onEndOfSpeech(IntPtr p, uint pos)
		{
			Console.WriteLine("OpusVad End SpeechEvent {0:D}ms", pos - mEOS_WINDOW_MS);
			eosDetected = true;
			eosPosition = pos;
		}
		static void onRecordingStarted()
        {
			sosDetected = eosDetected = false;
			sosPosition = eosPosition = 0; 
			mAudioBuffer = new Queue<byte[]>(mAUDIO_BUFFER_SIZE); 
		}
		static void stopRecognition()
        {
			// Here we can notify the recognizer that audio has finished prompting a final result
        }
		static void streamAudio(byte[] frame)
        {
			// This function represents the code where audio is sent to the recognizer
			//  (in the test case it's output to out.pcm)
			foreach (byte sample in frame)
			{
				binWriter?.Write(sample);
			}
		}
		static void onRecord(byte[] audioFrame)
        {
			// Here's your audio in function with live frames of audio from the microphone
			// expecting 20ms 16bit 16KHz linear PCM (320 samples 640 bytes)
			if (eosDetected)
            {
				// Ignore audio after EOS is detected
				stopRecognition();
				return;
            }
			// Send the audio to Opus
			// opusvad.ProcessAudio(audioFrame, 0, opusvad.FrameSize);
			OpusVAD.opusvad_process_audio(opusvad, audioFrame, OpusVAD.opusvad_get_frame_size(opusvad));

			// Queue data until connected and the RecognitionInitMessage is sent and speech is detected
			if (!connectionReady || !canStreamAudio || !sosDetected)
            {
				Console.Write(">");
				mAudioBuffer?.Enqueue(audioFrame);
            }
			// We have speech and we're ready to stream
			else if (mAudioBuffer?.Count > 0)
            {
				Console.WriteLine(">");
				Console.WriteLine("Sending queued audio {0:D} frames in the queue", mAudioBuffer.Count);
				mAudioBuffer.Enqueue(audioFrame);
				uint discardBufferCount = (sosPosition / mFRAME_SIZE_MS) -
										 (mLEADING_SILENCE_WINDOW_MS / mFRAME_SIZE_MS) -
										 (mSOS_WINDOW_MS / mFRAME_SIZE_MS);
				Console.WriteLine("Discarding {0:D} frames of queued audio", discardBufferCount);
				foreach (byte[] frame in mAudioBuffer)
                {
					if (--discardBufferCount < 0)
                    {
						Console.Write("<");
						streamAudio(frame);
                    }
                }
				mAudioBuffer.Clear();
            }
			else
            {
				Console.Write("(");
				streamAudio(audioFrame);
			}
		}

		static void Main(string[] args)
		{
			Console.WriteLine("Creating OpusVad");

            // Assign the callback handlers so prevent garbage collection
            _vadSOS = new OpusVAD.OpusVadCallback(onStartOfSpeech);
            _vadEOS = new OpusVAD.OpusVadCallback(onEndOfSpeech);
            // Set the OpusVAD options
            OpusVAD.OpusVADOptions options = new OpusVAD.OpusVADOptions
            {
                ctx = IntPtr.Zero,
                complexity = mDEFAULT_COMPLEXITY,
                bitRateType = OpusVAD.OPUSVAD_BIT_RATE_TYPE_CBR,
                sos = mSOS_WINDOW_MS,
                eos = mEOS_WINDOW_MS,
                speechDetectionSensitivity = mSENSITIVITY,
                onSOS = Marshal.GetFunctionPointerForDelegate(_vadSOS),
                onEOS = Marshal.GetFunctionPointerForDelegate(_vadEOS)
            };
			int error;
			opusvad = OpusVAD.opusvad_create(out error, ref options);
			Console.WriteLine("OpusVad created");

			// Read audio file into buffer for processing
			var stream = File.OpenRead(@"..\audio\test.pcm");
			var reader = new BinaryReader(stream);
			var fileLen = reader.BaseStream.Length - (OpusVAD.opusvad_get_frame_size(opusvad)*2); // Make it an even number of frames
			// Start the recording process
			onRecordingStarted();
			binWriter =
				new BinaryWriter(File.Open(@"..\audio\out.pcm", FileMode.Create));
			for (int i = 0; i < fileLen; i += (OpusVAD.opusvad_get_frame_size(opusvad)*2))
			{
				byte[] audioFrame = new byte[OpusVAD.opusvad_get_frame_size(opusvad)*2];
				for (int j=0; j<OpusVAD.opusvad_get_frame_size(opusvad)*2; j++)
                {
					audioFrame[j] = reader.ReadByte();
				}
				onRecord(audioFrame);
			}
			Console.WriteLine("OpusVADToolDotNet done");
			Console.ReadKey();
		}
	}
}
