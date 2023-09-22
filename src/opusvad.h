#pragma once

/*

Copyright 2016-2020 Nuance Communications, Inc.
 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
 
- Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
 
- Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

/** \mainpage
 * # How to build
 *
 * Currently the tool was tested on Linux and macOS, and should also compile
 * well on other platforms with small modifications.
 *
 * ## Opus
 *
 * 1. Optain the Opus 1.3.1 source code, and extract it to `opus-1.3.1` next to
 *    `OpusVADLib`.
 * 2. `cd opus-1.3.1 && patch -p1 < ../opus.patch`.
 * 3. Run `./configure`. You may need to add options based on the needs of your
 *    target architecture.
 * 4. `make`.
 *
 * ## OpusVADLib
 *
 * Simply run `make`. It should pick up the `libopus` library from
 * `../opus-1.3.1/.libs`. If you compiled or installed the library elsewhere,
 * adjust the `Makefile`.
 *
 * ## OpusVADTool
 *
 * Again, run `make`. To run the compiled program, you will need to set the
 * `LD_LIBRARY_PATH` environment variable (or `DYLD_LIBRARY_PATH` on macOS).
 *
 * ## Static Compilation
 *
 * If you prefer having a static executable or library, you might want to
 * link directly with `opus-1.3.1/.libs/libopus.a` and `OpusVADLib/opusvad.o`.
 *
 * To create a static executable on Linux with no dependency, you can use
 * [musl libc](http://www.musl-libc.org/). For such an example, look at the
 * `Makefile` of OpusVADTool: Set the `USEMUSL` environment variable to a
 * non-emtpy value, and run `make`.
 *
 * ## OpusVADJava
 *
 * This is a small example of using the library from Java using JNI.
 *
 * 1. In `OpusVADLib`, run `make clean libopusvadjava.so` (on macOS,
 *    `make clean libopusvadjava.dylib`).
 * 2. In `OpusVADJava`, run `mvn install`.
 * 3. Adjust `LD_LIBRARY_PATH` (or `DYLD_LIBRARY_PATH` on macOS), and
 *    run `java -jar target/OpusVADJava-0.0.1.jar in.pcm`.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPUSVAD_INTERNAL
typedef unsigned long long OpusVAD;
#endif

/** @defgroup OPUSVAD_error_codes Error codes
 * @{
 */
#define OPUSVAD_OK 0 /*!< No error */
#define OPUSVAD_ALLOC_FAILED -1 /*!< Memory allocation error */
#define OPUSVAD_OPUS_ERROR -2 /*!< Error with the Opus codec */
#define OPUSVAD_TOO_LARGE -3 /*!< Input memory given is too large */
#define OPUSVAD_TOO_SMALL -4 /*!< Ouput memory given is too small */
#define OPUSVAD_PARAM_ERROR -5 /*!< Parameter validation error */  
/**@}*/
 
/** @defgroup OPUSVAD_bit_rate_types Bit Rate Type flags
 * @{
 * The libopus encoder supports three different bit rate types:
 * 1. Variable Bit Rate (VBR)
 * 2. Constrained Variable Bit Rate (CVBR)
 * 3. Constant Bit Rate (CBR)
 * 
 * It is our recommendation to use CVBR to balance speed and compression when encoding the audio
 */
#define OPUSVAD_BIT_RATE_TYPE_VBR 0     /*!< Variable Bit Rate */
#define OPUSVAD_BIT_RATE_TYPE_CVBR 1    /*!< Constrained Variable Bit Rate. Default. */
#define OPUSVAD_BIT_RATE_TYPE_CBR 2     /*!< Constant Bit Rate */
/**@}*/

/** @defgroup OPUSVAD_functions OpusVAD functions
 * @{
 */
typedef void opusvad_callback(const void *ptr, unsigned int pos);

/** @defgroup OPUSVAD_options OpusVADOptions options
 * @{
 * #OpusVADOptions enable the client to fine-tune the behavior of the opusvad endpointer.
 * 
 * It is our recommendation to start with the default values for each option, and then tune them
 * based on how it performs against your end-to-end audio path and for your specific solution requirements.
 */
typedef struct opusvad_options {
    const void *ctx;        /*!< User defined pointer to be passed back on callbacks. */
    int complexity;         /*!< libopus VAD complexity setting. Valid values are between 0 - 10. Default: 3 */
    int bit_rate_type;      /*!< libopus VAD bit rate type setting. Valid values are 0, 1, and 2. Default: 1 (CVBR) */
    int sos;                /*!< Start of speech window in ms. Set to 0 to disable. Default: 220 */
    int eos;                /*!< End of speech window in ms. Set to 0 to disable. Default: 900 */
    int speech_detection_sensitivity; /*!< Sets sensitivity for start of speech detection. Valid values are between 0 - 100. Lower sensitivity requires fewer voiced speech frames to trigger start of speech. Default: 20 */
    opusvad_callback *onSOS;    /*!< Pointer to callback function notifying client when start of speech is detected. */
    opusvad_callback *onEOS;    /*!< Pointer to callback function notifying client when end of speech is detected. */
} OpusVADOptions;
/**@}*/

/** Creates the VAD Tool.
  * You should use #opusvad_destroy to dispose of the OpusVAD state memory.
  *
  * @warning You should not reuse the same OpusVAD state across multiple
  * separate audio streams, concurrently or sequentially.
  * @param [out] error `int*`: The error code.
  * @param [in] options `OpusVADOptions*` : optional pointer to Opus VAD options.
  * @returns The OpusVAD, or a null pointer if there was an error
  *          (inspect the error value).
  * @retval #OPUSVAD_OK : There was no error.
  * @retval #OPUSVAD_ALLOC_FAILED : Cannot allocate memory.
  * @retval #OPUSVAD_OPUS_ERROR : Error with OPUS.
  */
extern OpusVAD* opusvad_create(int *error, OpusVADOptions *options);
/**
  * @brief Optional create method to specify non-standard from duration (ms)
  * 
  * @param [out] error `int*`: The error code.
  * @param [in] options `OpusVADOptions*` : optional pointer to Opus VAD options.
  * @param frameDur `int`: frame duration ms, must be supported by Opus 5,10,20,40,60
  * @return OpusVAD* 
 */
extern OpusVAD* opusvad_create_opt(int *error, OpusVADOptions *options, int frameDur);

/** Get the number of samples expected in each frame.
  * The value should remain consistently the same for a given OpusVAD
  * instance.
  * @param [in] vad `OpusVAD*`: The VAD tool state.
  * @returns The number of samples per frame.
  */
extern int opusvad_get_frame_size(OpusVAD *vad);

/** Get the maximum distance back an event can refer.
  * Use this value as a minimum size for your audio buffer.
  * @param [in] vad `OpusVAD*`: The VAD tool state.
  * @returns The distance, in samples.
  */
extern int opusvad_get_max_buffer_samples(OpusVAD *vad);

/** Process some audio.
  * This normally expects exactly the number of samples returned by
  * #opusvad_get_frame_size.
  * If there are less, it is assumed the given samples
  * are followed by silence for the remainder of the frame. This is
  * normally done at the end of the input audio stream if the length of
  * the input audio isn't a multiple of the frame size.
  *
  * Each sample should a 16 bits, signed, little-endian, PCM mono
  * audio signal. The signal is clocked at 16000 samples per second.
  *
  * Following this function call, you can make use of the following to
  * get information about the audio up to this point:
  *
  * * #opusvad_get_vad_result
  * * #opusvad_get_opusencoded
  *
  * @param [in] vad `OpusVAD*`: The VAD tool state.
  * @param [in] frame `short*`: The input audio samples.
  * @param [in] num_samples `unsigned int`: The number of samples in `frame`.
  * @returns An error code, if any.
  * @retval #OPUSVAD_OK : There was no error.
  * @retval #OPUSVAD_TOO_LARGE: More samples than the frame size returned
  *                             by #opusvad_get_frame_size was given.
  */
extern int opusvad_process_audio(OpusVAD *vad, short *frame, unsigned int num_samples);

/** Process some audio encoded in IMA ADPCM.
  * This has the same behaviour as #opusvad_process_audio, but takes
  * as input IMA ADPCM samples.
  *
  * If high_nibble_first is set to 1, the bytes must be encoded with top
  * nibble first, then bottom nibble (n0n1 n2n3 ...). Otherwise, the bytes
  * must be encoded with low nibble first (n1n0 n3n2 ...).
  *
  * Each sample should be 4 bits ADPCM, mono. The signal is clocked at 16000
  * samples per second.
  *
  * This normally expects exactly the number of samples returned by
  * #opusvad_get_frame_size.
  * If there are less, it is assumed the given samples
  * are followed by silence for the remainder of the frame. This is
  * normally done at the end of the input audio stream if the length of
  * the input audio isn't a multiple of the frame size.
  * It is assumed that the number of samples is a multiple of 2 (so that
  * the memory size fits in whole bytes), and as such #opusvad_get_frame_size
  * is always guaranteed to be a multiple of 2.
  *
  * @param [in] vad `OpusVAD*`: The VAD tool state.
  * @param [in] frame `unsigned char*`: The input ADPCM samples.
  * @param [in] num_samples `unsigned int`: The number of samples in `frame`.
  * @returns An error code, if any.
  * @retval #OPUSVAD_OK : There was no error.
  * @retval #OPUSVAD_TOO_LARGE: More samples than the frame size returned
  *                             by #opusvad_get_frame_size was given.
  */
extern int opusvad_process_audio_adpcm(OpusVAD *vad, unsigned char *frame, unsigned int num_samples, int high_nibble_first);

/** Get the encoded OPUS frame.
  * The encoded OPUS frame should be at most #opusvad_get_frame_size * 2 bytes,
  * though usually much less.
  * @param [in] vad `OpusVAD*`: The VAD tool state.
  * @param [out] data `unsigned char*`: The OPUS encoded frame.
  * @param [in] max_bytes `unsigned int`: The maximum number of bytes that
  *                                       can be copied to `data`.
  * @returns The number of bytes in the OPUS encoded frame, or an error code.
  * @retval #OPUSVAD_TOO_SMALL: Not enough bytes for the OPUS encoded frame.
  */
extern int opusvad_get_opusencoded(OpusVAD *vad, unsigned char *data, unsigned int max_bytes);

/** Destroy the VAD Tool state
  * @param [in] vad `OpusVAD*`: The VAD tool state to destroy.
  * @returns An error code, if any.
  * @retval #OPUSVAD_OK : There was no error.
  */
extern int opusvad_destroy(OpusVAD *vad);

/** Set the OPUS encoder computational complexity.
  *
  * A higher complexity value will take more time to process the audio, but
  * the VAD can be potentially more accurate.
  *
  * @param [in] vad `OpusVAD*`: The VAD tool state.
  * @param [in] int The complexity. Smallest is 0, highest is 10
  * @returns An error code, if any.
  * @retval #OPUSVAD_OK : There was no error.
  * @retval #OPUSVAD_TOO_SMALL: The complexity value was smaller than 0.
  * @retval #OPUSVAD_TOO_LARGE: The complexity value was higher than 10.
  */
extern int opusvad_set_complexity(OpusVAD *vad, int complexity);

/**@}*/

#ifdef __cplusplus
}
#endif
