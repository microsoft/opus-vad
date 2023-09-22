package com.nuance.opusvad.jni;

public class OpusVADOptions {
        String ctx;
        int complexity = OpusVAD.DEFAULT_COMPLEXITY;
        int bitRateType = OpusVAD.DEFAULT_BIT_RATE_TYPE;
        int sosWindowMs = OpusVAD.DEFAULT_SOS_WINDOW_MS;
        int eosWindowMs = OpusVAD.DEFAULT_EOS_WINDOW_MS;
        int speechDetectionSensitivity = OpusVAD.DEFAULT_SPEECH_DETECTION_SENSITIVITY;

        public OpusVADOptions() {}
        
        public OpusVADOptions(String ctx, int complexity, int bitRateType, int sosWindowMs, int eosWindowMs, int speechDetectionSensitivity) {
            this.ctx = ctx;
            this.complexity = complexity;
            this.bitRateType = bitRateType;
            this.sosWindowMs = sosWindowMs;
            this.eosWindowMs = eosWindowMs;
            this.speechDetectionSensitivity = speechDetectionSensitivity;
        }
}
