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

#include <jni.h>
#include <stdlib.h>
#include <stdio.h>
#include "opusvad.h"

typedef struct {
    OpusVAD *vad;
} OpusVADWrapContext;

#define BYTEBUF_SIG "Ljava/nio/ByteBuffer;"
#define OPUSVADRES_SIG "Lcom/nuance/opusvad/jni/OpusVADResult;"
static JNIEnv *pEnv = NULL;
static jobject o;

void StoreContextPointer(JNIEnv *env, jobject instance, void *myPointer)
{
    jobject buf = (*env)->NewDirectByteBuffer(env, myPointer, 0);
    jclass class = (*env)->GetObjectClass(env, instance);
    jfieldID fieldid = (*env)->GetFieldID(env, class, "ctx", BYTEBUF_SIG);
    (*env)->SetObjectField(env, instance, fieldid, buf);
}

void NullifyContextPointer(JNIEnv *env, jobject instance)
{
    jclass class = (*env)->GetObjectClass(env, instance);
    jfieldID fieldid = (*env)->GetFieldID(env, class, "ctx", BYTEBUF_SIG);
    (*env)->SetObjectField(env, instance, fieldid, NULL);
}

void *GetContextPointer(JNIEnv *env, jobject instance)
{
    jclass class = (*env)->GetObjectClass(env, instance);
    jfieldID fieldid = (*env)->GetFieldID(env, class, "ctx", BYTEBUF_SIG);
    jobject buf = (*env)->GetObjectField(env, instance, fieldid);
    if (buf == NULL) {
        return NULL;
    }
    return (*env)->GetDirectBufferAddress(env, buf);
}

void opus_vad_sos(const void *ctx, unsigned int pos)
{
    jclass class = (*pEnv)->GetObjectClass(pEnv, o);
    jmethodID callback = (*pEnv)->GetMethodID(pEnv, class, "onStartOfSpeech", "(Ljava/lang/String;I)V");

    jstring javaString = (*pEnv)->NewStringUTF(pEnv, (const char *)ctx);
    (*pEnv)->CallVoidMethod(pEnv, o, callback, javaString, pos);
}

void opus_vad_eos(const void *ctx, unsigned int pos)
{
    jclass class = (*pEnv)->GetObjectClass(pEnv, o);
    jmethodID callback = (*pEnv)->GetMethodID(pEnv, class, "onEndOfSpeech", "(Ljava/lang/String;I)V");

    jstring javaString = (*pEnv)->NewStringUTF(pEnv, (const char *)ctx);
    (*pEnv)->CallVoidMethod(pEnv, o, callback, javaString, pos);
}

JNIEXPORT jint JNICALL Java_com_nuance_opusvad_jni_OpusVAD_init(JNIEnv *env, jobject instance, jobject options)
{
    if (GetContextPointer(env, instance) != NULL) {
        OpusVADWrapContext *ctx = GetContextPointer(env, instance);
        opusvad_destroy(ctx->vad);
        free(ctx);
//        return -1;
    }
    OpusVADWrapContext *ctx = (OpusVADWrapContext*)malloc(sizeof(OpusVADWrapContext));
    StoreContextPointer(env, instance, ctx);
    int error;

    OpusVADOptions *opts = NULL;
    if (options != NULL) {
        jclass optionsClass = (*env)->GetObjectClass(env, options);
        if (optionsClass) {
            opts = malloc(sizeof(OpusVADOptions));
            opts->ctx = NULL;
            
            jfieldID context = (*env)->GetFieldID(env, optionsClass, "ctx", "Ljava/lang/String;");
            jstring jstrCtx = (jstring) (*env)->GetObjectField(env, options, context);
            if (jstrCtx != NULL) {
                opts->ctx = (*env)->GetStringUTFChars(env, jstrCtx, 0);
            }

            jfieldID complexity = (*env)->GetFieldID(env, optionsClass, "complexity", "I");
            opts->complexity = (*env)->GetIntField(env, options, complexity);

            jfieldID bit_rate_type = (*env)->GetFieldID(env, optionsClass, "bitRateType", "I");
            opts->bit_rate_type = (*env)->GetIntField(env, options, bit_rate_type);

            jfieldID sos = (*env)->GetFieldID(env, optionsClass, "sosWindowMs", "I");
            opts->sos = (*env)->GetIntField(env, options, sos);

            jfieldID eos = (*env)->GetFieldID(env, optionsClass, "eosWindowMs", "I");
            opts->eos = (*env)->GetIntField(env, options, eos);

            jfieldID speech_detection_sensitivity = (*env)->GetFieldID(env, optionsClass, "speechDetectionSensitivity", "I");
            opts->speech_detection_sensitivity = (*env)->GetIntField(env, options, speech_detection_sensitivity);

            opts->onSOS = opus_vad_sos;
            opts->onEOS = opus_vad_eos;
        }
    }

    pEnv = env;
    o=(*env)->NewGlobalRef(env, instance);
    ctx->vad = opusvad_create(&error, opts);
// cleblanc
    free(opts);
    return error;
}

JNIEXPORT jint JNICALL Java_com_nuance_opusvad_jni_OpusVAD_setComplexity(JNIEnv *env, jobject instance, jint complexity)
{
    OpusVADWrapContext *ctx = GetContextPointer(env, instance);
    return opusvad_set_complexity(ctx->vad, complexity);
}

JNIEXPORT jint JNICALL Java_com_nuance_opusvad_jni_OpusVAD_processAudio(JNIEnv *env, jobject instance, jobject audiobuf, jint num_samples)
{
    OpusVADWrapContext *ctx = GetContextPointer(env, instance);
    pEnv = env;
    o=(*env)->NewGlobalRef(env, instance);
    short *inbuf = (*env)->GetDirectBufferAddress(env, audiobuf);
    return opusvad_process_audio(ctx->vad, inbuf, num_samples);
}

JNIEXPORT jint JNICALL Java_com_nuance_opusvad_jni_OpusVAD_processAudioAdpcm(JNIEnv *env, jobject instance, jobject audiobuf, jint num_samples, jint high_nibble_first)
{
    OpusVADWrapContext *ctx = GetContextPointer(env, instance);
    pEnv = env;
    o=(*env)->NewGlobalRef(env, instance);
    unsigned char *inbuf = (*env)->GetDirectBufferAddress(env, audiobuf);
    return opusvad_process_audio_adpcm(ctx->vad, inbuf, num_samples, high_nibble_first);
}

JNIEXPORT jint JNICALL Java_com_nuance_opusvad_jni_OpusVAD_getFrameSize(JNIEnv *env, jobject instance)
{
    OpusVADWrapContext *ctx = GetContextPointer(env, instance);

    return opusvad_get_frame_size(ctx->vad);
}

JNIEXPORT jint JNICALL Java_com_nuance_opusvad_jni_OpusVAD_getMaxBufferSamples(JNIEnv *env, jobject instance)
{
    OpusVADWrapContext *ctx = GetContextPointer(env, instance);

    return opusvad_get_max_buffer_samples(ctx->vad);
}

JNIEXPORT jint JNICALL Java_com_nuance_opusvad_jni_OpusVAD_getOpusEncoded(JNIEnv *env, jobject instance, jobject outbuf)
{
    OpusVADWrapContext *ctx = GetContextPointer(env, instance);
    int outBufLen = (*env)->GetDirectBufferCapacity(env, outbuf);
    unsigned char *outBufMem = (*env)->GetDirectBufferAddress(env, outbuf);
    return opusvad_get_opusencoded(ctx->vad, outBufMem, outBufLen);
}
