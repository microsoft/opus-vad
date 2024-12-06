using System;
using System.Runtime.InteropServices;

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
