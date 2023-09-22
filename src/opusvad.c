/*

Copyright 2016-2017 Nuance Communications, Inc.

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

#include <math.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#ifdef TARGET_EMSCRIPTEN
  #include <emscripten.h>
#endif

#include <stdio.h>
#include <pthread.h>
#include "opus.h"
#include "adpcm.h"
#include "dequeue.h"

#define OPUSVAD_INTERNAL
typedef void _opusvad_callback(const void *ptr, unsigned int pos);

typedef struct OpusVAD {
    OpusEncoder *opus;
    opus_int16 *in_samples;
    unsigned char *out_frame;
    unsigned int out_frame_bytes;
    int cur_frame_num;

    int complexity;
    int bit_rate_type;

    int sos_window_num_speech_frames;       // count of voiced and unvoiced audio frames
    int sos_window_num_voiced_frames;       // count of voiced audio frames
    int eos_window_num_nonspeech_frames;    // count of no voiced or unvoiced audio frames
    struct dequeue *sos_window;
    struct dequeue *eos_window;
    double speech_saturation_threshold;
    double speech_detection_sensitivity;
    int sos_detected;
    int eos_detected;

    struct adpcm_state adpcm_state;

    _opusvad_callback * pSOS;
    _opusvad_callback * pEOS;
    const void *ctx;
} OpusVAD;


#include "opusvad.h"

#define INFREQ 16000
#define INSAMPLESIZE sizeof(opus_int16)
#define INBYTESPERSEC (INFREQ * INSAMPLESIZE)
#define OUTBUFSIZE 4000
static int FRAMEDUR = 20;

#define TYPE_NO_VOICE_ACTIVITY 0
#define TYPE_UNVOICED 1
#define TYPE_VOICED 2

#define STATE_NO_STATE 0
#define STATE_IN_SPEECH 1
#define STATE_LOOKING_FOR_EOS 2
#define STATE_END_OF_SPEECH 3

#define DEFAULT_COMPLEXITY 3
#define DEFAULT_BIT_RATE_TYPE OPUSVAD_BIT_RATE_TYPE_CVBR

#define DEFAULT_SOS_WINDOW_MS 220
#define DEFAULT_EOS_WINDOW_MS 1280

#define DEFAULT_SPEECH_SATURATION_THRESHOLD 0.85
#define SPEECH_SATURATION_THRESHOLD_TRIGGER 7   // Require at least 140ms of audio for saturation threshold to be applied
#define DEFAULT_SPEECH_DETECTION_SENSITIVITY 0.2
#define DEFAULT_NON_SPEECH_SATURATION_THRESHOLD 0.88

static int SOS_FRAME_THRESHOLD = (DEFAULT_SOS_WINDOW_MS/20);
static int EOS_FRAME_THRESHOLD = (DEFAULT_EOS_WINDOW_MS/20);

#define SOS_PAD_FRAMES 2
#define EOS_PAD_FRAMES 2

#define FALSE 0
#define TRUE 1

void _log(const char *fmt, ...) {
    #ifdef DEBUG
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    #endif
}

#ifdef TARGET_EMSCRIPTEN
EMSCRIPTEN_KEEPALIVE
#endif
int opusvad_get_frame_size(OpusVAD *vad)
{
    return INFREQ * FRAMEDUR / 1000;
}

#ifdef TARGET_EMSCRIPTEN
EMSCRIPTEN_KEEPALIVE
#endif
int frame_bytes(OpusVAD *vad)
{
    return opusvad_get_frame_size(vad) * INSAMPLESIZE;
}

#ifdef TARGET_EMSCRIPTEN
EMSCRIPTEN_KEEPALIVE
#endif
int opusvad_get_max_buffer_samples(OpusVAD *vad)
{
    return (SOS_PAD_FRAMES + SOS_FRAME_THRESHOLD + 1) * opusvad_get_frame_size(vad);
}

OpusVAD* _init_opusvad() {
    OpusVAD* vad = (OpusVAD*)malloc(sizeof(OpusVAD));
    if (vad != NULL)
    {
        memset(vad, 0, sizeof(OpusVAD));
    }
    return vad;
}

// Initialize opusvad options
void _init_opusvad_params(OpusVAD *vad, OpusVADOptions *options) {
    
    // Initialize with defaults
    vad->complexity = DEFAULT_COMPLEXITY;
    vad->bit_rate_type = DEFAULT_BIT_RATE_TYPE;
    vad->speech_detection_sensitivity = DEFAULT_SPEECH_DETECTION_SENSITIVITY;
    vad->pSOS = NULL;
    vad->pEOS = NULL;
    vad->ctx = NULL;

    if(options != NULL) {
        if (options->complexity >= 0 && options->complexity <= 10) {
            vad->complexity = options->complexity;
        }
        if (options->bit_rate_type >= 0 && options->bit_rate_type <= 2) {
            vad->bit_rate_type = options->bit_rate_type;
        }
        vad->pSOS = options->onSOS;
        vad->pEOS = options->onEOS;
        if (options->speech_detection_sensitivity >= 0 && options->speech_detection_sensitivity <= 100) {
            vad->speech_detection_sensitivity = options->speech_detection_sensitivity / 100.0;
        }
        if (options->sos >= 0) {
            SOS_FRAME_THRESHOLD = ceil(options->sos/FRAMEDUR);
        } else {
            SOS_FRAME_THRESHOLD = DEFAULT_SOS_WINDOW_MS/FRAMEDUR;
        }
        if (options->eos >= 0) {
            EOS_FRAME_THRESHOLD = ceil(options->eos/FRAMEDUR);
        } else {
            EOS_FRAME_THRESHOLD = DEFAULT_EOS_WINDOW_MS/FRAMEDUR;
        }
        vad->ctx = options->ctx;
    }
}

// Check to see if frame contains any speech (voiced or unvoiced)
int _frame_contains_speech(int voice_signal) {
    return (voice_signal == TYPE_NO_VOICE_ACTIVITY) ? FALSE : TRUE;
}

// Check to see if frame contains a voiced speech signal
int _frame_contains_voiced_data(int voice_signal) {
    return (voice_signal == TYPE_VOICED) ? TRUE : FALSE;
}

// Shift the SoS window right
void _shift_sos_window(OpusVAD *vad, int voice_signal) {

    // Don't bother shifting window if threshold is not greather than 0
    if (SOS_FRAME_THRESHOLD <= 0) {
        return;
    }

    // Update counts for incoming voice signal
    if (_frame_contains_speech(voice_signal)) {
        vad->sos_window_num_speech_frames++;
        if (_frame_contains_voiced_data(voice_signal)) {
            vad->sos_window_num_voiced_frames++;
        }
    }

    // Update counts for outgoing voice signal
    if( full(vad->sos_window) ) {
        // Remove trailing signal from window
        int trailing_signal = dequeueF(vad->sos_window);

        if (_frame_contains_speech(trailing_signal)) {
            vad->sos_window_num_speech_frames--;
            if (_frame_contains_voiced_data(trailing_signal)) {
                vad->sos_window_num_voiced_frames--;
            }
        }
    }

    // Slide window to the right
    enqueueR(vad->sos_window, voice_signal);
}

// Check to see if SoS is detected in the windowed frames
int _is_sos_detected(OpusVAD *vad) {

    // If SoS window is disabled, trigger SoS event immediately
    if (SOS_FRAME_THRESHOLD <= 0) {
        return TRUE;
    }

    // For SoS to be triggered, the sos window must contain:
    //  1. enough speech frames (voiced or unvoiced) in relation to number of non-speech frames, and
    //  2. enough voiced speech frames within the total number of speech frames
    if (full(vad->sos_window) && vad->sos_window_num_speech_frames > 0) {
        double pct_speech = (1.0 * vad->sos_window_num_speech_frames) / (1.0 * SOS_FRAME_THRESHOLD);
        double pct_voiced = (1.0 * vad->sos_window_num_voiced_frames) / (1.0 * vad->sos_window_num_speech_frames);
        return (pct_speech >= vad->speech_saturation_threshold && pct_voiced >= vad->speech_detection_sensitivity) ? TRUE : FALSE;
    }
    return FALSE;
}

// Shift the EoS window right
void _shift_eos_window(OpusVAD *vad, int voice_signal) {

    // Don't bother shifting window if threshold is not greather than 0
    if (EOS_FRAME_THRESHOLD <= 0) {
        return;
    }

    // Update frame counts for incoming voice signal
    if (!_frame_contains_speech(voice_signal)) {
        vad->eos_window_num_nonspeech_frames++;
    }

    // Update frame counts for outgoing voice signal
    if( full(vad->eos_window) ) {
        // Remove trailing signal from window
        int trailing_signal = dequeueF(vad->eos_window);
        if (!_frame_contains_speech(trailing_signal)) {
            vad->eos_window_num_nonspeech_frames--;
        }
    }

    // Slide window to the right
    enqueueR(vad->eos_window, voice_signal);
}

// Check to see if EoS is detected in the windows frames
int _is_eos_detected(OpusVAD *vad) {

    // If EoS window is disabled, never trigger an EoS event
    if (EOS_FRAME_THRESHOLD <= 0) {
        return FALSE;
    }

    // Trigger an EoS event if percent of non-speech frames exceeds saturation level
    if (full(vad->eos_window) && vad->eos_window_num_nonspeech_frames > 0) {
        double pct_nonspeech = (1.0 * vad->eos_window_num_nonspeech_frames) / (1.0 * EOS_FRAME_THRESHOLD);
        return (pct_nonspeech >= DEFAULT_NON_SPEECH_SATURATION_THRESHOLD) ? TRUE : FALSE;
    }
    return FALSE;
}

struct arg_struct {
    const void *ctx;
    int pos;
};

// Notify the SoS/EoS callback handler
void _notify_callback(const void *ctx, _opusvad_callback *cb, int pos) {    

    if (cb) {
        (*cb)(ctx, pos);
    }

}

#ifdef TARGET_EMSCRIPTEN
EMSCRIPTEN_KEEPALIVE
#endif
OpusVAD* opusvad_create_opt(int *error, OpusVADOptions *options, int frameDur) {
    FRAMEDUR = frameDur;
    return opusvad_create(error, options);
}

#ifdef TARGET_EMSCRIPTEN
EMSCRIPTEN_KEEPALIVE
#endif
OpusVAD* opusvad_create(int *error, OpusVADOptions *options)
{
    if (error == NULL){
        _log("Invalid error paramter - cannot be null");
        return NULL;
    }

    OpusVAD* vad = _init_opusvad();
    if (vad == NULL) {
        *error = OPUS_ALLOC_FAIL;
        return NULL;
    }

    _init_opusvad_params(vad, options);

    int opuserror;
    vad->opus = opus_encoder_create(INFREQ, 1, OPUS_APPLICATION_VOIP, &opuserror);
    if (opuserror != 0)
    {
        if (error != NULL)
        {
            *error = OPUSVAD_OPUS_ERROR;
            return NULL;
        }
    }

    int dummy;
    opuserror = opus_encoder_ctl(vad->opus, OPUS_GET_PREVSIGNALTYPE(&dummy));
    if (opuserror != OPUS_OK) {
        *error = OPUSVAD_OPUS_ERROR;
        return NULL;
    }

    opus_encoder_ctl(vad->opus, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));

    // Set Complexity to 3 and enable Constrained VBR to align with PS best practices
    opus_encoder_ctl(vad->opus, OPUS_SET_VBR_CONSTRAINT(vad->bit_rate_type));
    opus_encoder_ctl(vad->opus, OPUS_SET_COMPLEXITY(vad->complexity));

    // Initialize input and output buffers
    vad->in_samples = (short*)malloc(frame_bytes(vad));
    if (vad->in_samples == NULL)
    {
        if (error != NULL)
        {
            *error = OPUSVAD_ALLOC_FAILED;
        }
        opusvad_destroy((OpusVAD*)vad);
        return NULL;
    }

    vad->out_frame = (unsigned char*)malloc(OUTBUFSIZE);
    if (vad->out_frame == NULL)
    {
        if (error != NULL)
        {
            *error = OPUSVAD_ALLOC_FAILED;
        }
        opusvad_destroy((OpusVAD*)vad);
        return NULL;
    }

    // Initialize frame counts
    vad->cur_frame_num = 0;

    // Initialize windowing
    vad->sos_window_num_speech_frames = 0;
    vad->speech_saturation_threshold = (SOS_FRAME_THRESHOLD < SPEECH_SATURATION_THRESHOLD_TRIGGER) ? 1.0 : DEFAULT_SPEECH_SATURATION_THRESHOLD; // For small windows, require 100% speech
    vad->sos_window = malloc(sizeof(*vad->sos_window) + (sizeof(int)*SOS_FRAME_THRESHOLD));
    vad->sos_window->max = SOS_FRAME_THRESHOLD;
    initialize(vad->sos_window);

    vad->eos_window_num_nonspeech_frames = 0;
    vad->eos_window = malloc(sizeof(*vad->eos_window) + (sizeof(int)*EOS_FRAME_THRESHOLD));
    vad->eos_window->max = EOS_FRAME_THRESHOLD;
    initialize(vad->eos_window);

    *error = OPUSVAD_OK;
    
    return vad;
}

#ifdef TARGET_EMSCRIPTEN
EMSCRIPTEN_KEEPALIVE
#endif
int opusvad_set_complexity(OpusVAD *vad, int complexity)
{
    if (complexity < 0)
    {
        return OPUSVAD_TOO_SMALL;
    }
    if (complexity > 10)
    {
        return OPUSVAD_TOO_LARGE;
    }

    opus_encoder_ctl(vad->opus, OPUS_SET_COMPLEXITY(complexity));

    return OPUSVAD_OK;
}

#ifdef TARGET_EMSCRIPTEN
EMSCRIPTEN_KEEPALIVE
#endif
int opusvad_destroy(OpusVAD *vad)
{
    if (vad != NULL)
    {
        if (vad->in_samples != NULL)
        {
            free(vad->in_samples);
        }
        if (vad->out_frame != NULL)
        {
            free(vad->out_frame);
        }
        if (vad->sos_window != NULL) {
            free(vad->sos_window);
        }
        if (vad->eos_window != NULL) {
            free(vad->eos_window);
        }
        if (vad->opus != NULL)
        {
            free(vad->opus);
        }
        free(vad);
    }
    return OPUSVAD_OK;
}

#ifdef TARGET_EMSCRIPTEN
EMSCRIPTEN_KEEPALIVE
#endif
int opusvad_process_audio(OpusVAD *vad, short *frame, unsigned int num_samples)
{
    int framesize = opusvad_get_frame_size(vad);
    opus_int32 cur_signal_type;
    int error;

    if (num_samples > framesize)
    {
        return OPUSVAD_TOO_LARGE;
    }

    vad->cur_frame_num++;

    // Encode the audio and get VAD signal type
    vad->out_frame_bytes = opus_encode(vad->opus, frame, num_samples, vad->out_frame, OUTBUFSIZE);
    error = opus_encoder_ctl(vad->opus, OPUS_GET_PREVSIGNALTYPE(&cur_signal_type));
    if (error != OPUS_OK) {
        return OPUSVAD_OPUS_ERROR;
    }

    #ifdef DEBUG
    switch (cur_signal_type)
    {
    case TYPE_VOICED:
        _log("%d SIGNAL TYPE: %s\n", vad->cur_frame_num * FRAMEDUR, "VOICED");
        break;
    case TYPE_UNVOICED:
        _log("%d SIGNAL TYPE: %s\n", vad->cur_frame_num * FRAMEDUR, "UNVOICED");
        break;
    case TYPE_NO_VOICE_ACTIVITY:
        _log("%d SIGNAL TYPE: %s\n", vad->cur_frame_num * FRAMEDUR, "NO_VOICE_ACTIVITY");
        break;
    default:
        _log("%d SIGNAL TYPE: %s\n", vad->cur_frame_num * FRAMEDUR, "UNKNOWN");
        break;
    }
    #endif

    // sliding window algorithm
    if (!vad->sos_detected) {
        _shift_sos_window(vad, cur_signal_type);
        // check for sos
        if (_is_sos_detected(vad)) {
            vad->sos_detected = TRUE;
            int pos = (vad->cur_frame_num - (SOS_FRAME_THRESHOLD* vad->speech_detection_sensitivity)) * FRAMEDUR;
            _log("%d SOS DETECTED [pos: %d]\n", vad->cur_frame_num * FRAMEDUR, (pos < 0) ? 0 : pos);
            _notify_callback(vad->ctx, vad->pSOS, ((pos < 0) ? 0 : pos));
        }
    } else if (!vad->eos_detected) {
        _shift_eos_window(vad, cur_signal_type);
        // check for eos
        if (_is_eos_detected(vad)) {
            vad->eos_detected = TRUE;
            int pos = (vad->cur_frame_num + (EOS_FRAME_THRESHOLD * vad->speech_detection_sensitivity)) * FRAMEDUR;
            _log("%d EOS DETECTED [pos: %d]\n", vad->cur_frame_num * FRAMEDUR, (pos < 0) ? 0 : pos);
            _notify_callback(vad->ctx, vad->pEOS, (pos < 0) ? 0 : pos);
        }
    }
    
    //vad->prev_signal_type = cur_signal_type;

    return OPUSVAD_OK;
}

#ifdef TARGET_EMSCRIPTEN
EMSCRIPTEN_KEEPALIVE
#endif
int opusvad_process_audio_adpcm(OpusVAD *vad, unsigned char *frame, unsigned int num_samples, int high_nibble_first)
{
    int framesize = opusvad_get_frame_size(vad);
    if (num_samples > framesize)
    {
        return OPUSVAD_TOO_LARGE;
    }
    adpcm_decoder((char*)frame, vad->in_samples, num_samples, &vad->adpcm_state, high_nibble_first);
    if (num_samples < framesize)
    {
        int numbytes = num_samples * INSAMPLESIZE;
        memset((char*)vad->in_samples + numbytes, 0, frame_bytes(vad) - numbytes);
    }
    return opusvad_process_audio(vad, vad->in_samples, framesize);
}

#ifdef TARGET_EMSCRIPTEN
EMSCRIPTEN_KEEPALIVE
#endif
int opusvad_get_opusencoded(OpusVAD *vad, unsigned char *data, unsigned int max_bytes)
{
    if (vad->out_frame_bytes >= max_bytes)
    {
        return OPUSVAD_TOO_SMALL;
    }

    memcpy(data, vad->out_frame, vad->out_frame_bytes);

    return vad->out_frame_bytes;
}
