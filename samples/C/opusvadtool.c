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

#include "opusvad.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <uuid/uuid.h>
#include <unistd.h>
#include <getopt.h>

#define INSAMPLESIZE 2
#define ENCODED_BYTES 20000

volatile long g_sos = 0;
volatile long g_eos = 0;

void opus_vad_sos(const void *p, unsigned int pos)
{
    g_sos = pos;
      printf("[%s] sos: %dms\n", (char *)p, pos);
}

void opus_vad_eos(const void *p, unsigned int pos)
{
    g_eos = pos;
      printf("[%s] eos: %dms\n", (char *)p, pos);
}

void print_usage(char **argv) {
    printf("Usage: %s [-h] -f <infile> [-s sos] [-e eos] [-c complexity] [-b bit_rate_type] [-t speech sensitivity threshold] [-a] [-n]\n", argv[0]);
    printf("Input file must be 16000 Hz 16 bit signed little-endian PCM mono\n");
    printf("If <infile> is \"-\", input is read from standard input\n");
    printf("-s \t start of speech window in ms. Default: 220\n");
    printf("-e \t end of speech window in ms. Default: 900\n");
    printf("-c \t opus vad encoding complexity level (0-10). Default: 3\n");
    printf("-b \t opus vad bit rate type (0 = VBR, 1 = CVBR, 2 = CBR). Default: 1\n");
    printf("-t \t speech detection sensitivity parameter (0-100). Specify 0 for least sensitivity, 100 for most. Default: 20\n");
    printf("-a \t <infile> is treated as IMA-ADPCM 4bit 16kHz\n");
    printf("-l \t <outfile> to append results to\n");
    printf("-n \t specify high-nibble order for adpcm encoded <infile>\n");
    printf("Examples:\n\tsox in.wav -r 16000 -b 16 -e signed -L -c 1 -t raw - | %s -f - -e 400\n", argv[0]);
    printf("\tsox in.wav -t ima -e ima-adpcm -r 16000 -c 1 - | %s -f -a\n", argv[0]);
    printf("\tsox in.wav -t ima -e ima-adpcm -r 16000 -c 1 -N - | %s -f - -a -n\n", argv[0]);
    printf("\tsox in.wav -r 16000 -b 16 -e signed -L -c 1 -t raw - | %s -f - -t 30 -e 700\n", argv[0]);
}

int main(int argc, char **argv)
{
    int error = 0;
    OpusVAD *vad;
    char *inbuf;
    FILE *fd;
    char lastpacket = 0;
    size_t read = 0;
    clock_t startclock, endclock;
    int frame_size;
    int use_adpcm = 0;
    int adpcm_high_nibble_first = 0;
    int sos = 220;
    int eos = 900;
    int complexity = 3;
    int bit_rate_type = 1;
    int sensitivity = 20;
    char *input = "";
    char *log_filename = NULL;
    FILE *log_file = NULL;
    int once = 1;
    short backup_buffer[20][320]; // about 400ms
    short bb_index = 0;

    int file_num = 0;
    FILE *audioFile = NULL;
    char *fName = "speech_%d.pcm";

    int option;
    while((option = getopt(argc, argv, ":f:s:e:c:b:t:l:han")) != -1){
        switch(option){
            case 'f':
                input = optarg;
                break;
            case 'l':
                log_filename = optarg;
                break;
            case 'h':
                print_usage(argv);
                return 1;
            case 's':
                sos = atoi(optarg);
                break;
            case 'e':
                eos = atoi(optarg);
                break;
            case 'c':
                complexity = atoi(optarg);
                break;
            case 'b':
                bit_rate_type = atoi(optarg);
                break;
            case 't':
                sensitivity = atoi(optarg);
                break;
            case 'a':
                use_adpcm = 1;
                break;
            case 'n':
                adpcm_high_nibble_first = 1;
                break;
            case ':':
                printf("option needs a value\n");
                break;
            case '?': //used for some unknown options
                printf("unknown option: %c\n", optopt);
                break;
        }
    }

    if (!input) {
        printf("Missing required option: f");
        return -1;
    }

    if (log_filename != NULL) {
        log_file = fopen(log_filename, "a");
    }
    uuid_t binuuid;
    uuid_generate_random(binuuid);
    char *ctx = malloc(37);
    uuid_unparse_lower(binuuid, ctx);

    OpusVADOptions opts = {
        (const void *)ctx,
        complexity,
        bit_rate_type,
        sos,
        eos,
        sensitivity,
        opus_vad_sos,
        opus_vad_eos
    };
    startclock = clock();
    vad = opusvad_create(&error, &opts);
    if (error != OPUSVAD_OK)
    {
        printf("Error with opusvad_create: %d\n", error);
        return 2;
    }
    frame_size = use_adpcm ? 96*5 : opusvad_get_frame_size(vad);

    fd = (!strncmp(input, "-", 2)) ? stdin : fopen(input, "rb");
    inbuf = (use_adpcm) ? malloc(frame_size / 2) : malloc(frame_size * INSAMPLESIZE);
    char name[256];

    while (!lastpacket)
    {
        read = (use_adpcm) ? fread(inbuf, 1, frame_size / 2, fd) : fread(inbuf, INSAMPLESIZE, frame_size, fd);
        if (feof(fd))
        {
            lastpacket = 1;
            if (read == 0) {
                break;
            }
        }

        if( use_adpcm == 1 )
            error = opusvad_process_audio_adpcm(vad, (unsigned char*)inbuf, read * 2, adpcm_high_nibble_first);
        else
            error = opusvad_process_audio(vad, (short*)inbuf, read / INSAMPLESIZE);

        memcpy(backup_buffer[bb_index++], inbuf, read*INSAMPLESIZE);
        if (bb_index == 20) {
            bb_index = 0;
        }
        if (g_sos > 0) {
            if (once) {
                sprintf(name, fName, file_num++);
                printf("Opened File %s\n", name);
                printf("\n\n");
                audioFile = fopen(name, "wb");

                fwrite(backup_buffer[bb_index], INSAMPLESIZE, 320*(20-bb_index), audioFile);
                fwrite(backup_buffer[0], INSAMPLESIZE, 320*bb_index, audioFile);
                once = 0;
            }
            fwrite(inbuf, INSAMPLESIZE, read, audioFile);
        }

        if (g_eos != 0 ) {
            fclose(audioFile);
                opusvad_destroy(vad);
                vad = opusvad_create(&error, &opts);
                bb_index = g_sos = g_eos = 0;
                once = 1;
                for(int z=0;z<20;z++)
                    memset(backup_buffer[z], 0, 320 * sizeof(backup_buffer[0][0]));
        }
        if (error != OPUSVAD_OK)
        {
            printf("Error with opusvad_process_audio: %d\n", error);
            return 3;
        }
    }
    fclose(audioFile);
    endclock = clock();
    printf("Time: %.4f seconds\n", (double)(endclock - startclock) / CLOCKS_PER_SEC);
    if (log_file != NULL) {
        fprintf(log_file, "%s,%ld,%ld\n",input,g_sos,g_eos);
        fclose(log_file);
    }

    fclose(fd);
    free(inbuf);
    opusvad_destroy(vad);

    return 0;
}
