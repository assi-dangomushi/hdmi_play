/*
Copyright (c) 2012, Broadcom Europe Ltd
Copyright (c) 2015-2019, Amanogawa Audio Labo
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
Original file is /opt/vc/src/hello_pi/hello_audio/aucdio.c (Raspberry pi)
Modified by Amanogawa Audio Lab
*/

// hdmi_play.c version 0.13


// Audio output demo using OpenMAX IL though the ilcient helper library

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <semaphore.h>

#include "bcm_host.h"
#include "ilclient.h"

#define OUT_CHANNELS(num_channels) ((num_channels) > 4 ? 8: (num_channels) > 2 ? 4: (num_channels))

#define BUFFER_SIZE_SAMPLES 1024
#define BN 20
#define SLEEPTIME 5*1000

#define DEBUG_PRINT(...)  fprintf(stderr, __VA_ARGS__)
#define N 80

typedef int int32_t;

typedef struct {
   sem_t sema;
   ILCLIENT_T *client;
   COMPONENT_T *audio_render;
   COMPONENT_T *list[2];
   OMX_BUFFERHEADERTYPE *user_buffer_list; // buffers owned by the client
   uint32_t num_buffers;
   uint32_t bytes_per_sample;
} AUDIOPLAY_STATE_T;

static void input_buffer_callback(void *data, COMPONENT_T *comp)
{
   // do nothing - could add a callback to the user
   // to indicate more buffers may be available.
}

int32_t audioplay_create(AUDIOPLAY_STATE_T **handle,
                         uint32_t sample_rate,
                         uint32_t num_channels,
                         uint32_t bit_depth,
                         uint32_t num_buffers,
                         uint32_t buffer_size)
{
   uint32_t bytes_per_sample = (bit_depth * OUT_CHANNELS(num_channels)) >> 3;
   int32_t ret = -1;

   *handle = NULL;

   // basic sanity check on arguments
   if(sample_rate >= 8000 && sample_rate <= 192000 &&
      (num_channels >= 1 && num_channels <= 8) &&
      (bit_depth == 16 || bit_depth == 32) &&
      num_buffers > 0 &&
      buffer_size >= bytes_per_sample)
   {
      // buffer lengths must be 16 byte aligned for VCHI
      int size = (buffer_size + 15) & ~15;
      AUDIOPLAY_STATE_T *st;

      // buffer offsets must also be 16 byte aligned for VCHI
      st = calloc(1, sizeof(AUDIOPLAY_STATE_T));

      if(st)
      {
         OMX_ERRORTYPE error;
         OMX_PARAM_PORTDEFINITIONTYPE param;
         OMX_AUDIO_PARAM_PCMMODETYPE pcm;
         int32_t s;

         ret = 0;
         *handle = st;

         // create and start up everything
         s = sem_init(&st->sema, 0, 1);
         assert(s == 0);

         st->bytes_per_sample = bytes_per_sample;
         st->num_buffers = num_buffers;

         st->client = ilclient_init();
         assert(st->client != NULL);

         ilclient_set_empty_buffer_done_callback(st->client, input_buffer_callback, st);

         error = OMX_Init();
         assert(error == OMX_ErrorNone);

         ilclient_create_component(st->client, &st->audio_render, "audio_render", ILCLIENT_ENABLE_INPUT_BUFFERS | ILCLIENT_DISABLE_ALL_PORTS);
         assert(st->audio_render != NULL);

         st->list[0] = st->audio_render;

         // set up the number/size of buffers
         memset(&param, 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
         param.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
         param.nVersion.nVersion = OMX_VERSION;
         param.nPortIndex = 100;

         error = OMX_GetParameter(ILC_GET_HANDLE(st->audio_render), OMX_IndexParamPortDefinition, &param);
         assert(error == OMX_ErrorNone);

         param.nBufferSize = size;
         param.nBufferCountActual = num_buffers;

         error = OMX_SetParameter(ILC_GET_HANDLE(st->audio_render), OMX_IndexParamPortDefinition, &param);
         assert(error == OMX_ErrorNone);

         // set the pcm parameters
         memset(&pcm, 0, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
         pcm.nSize = sizeof(OMX_AUDIO_PARAM_PCMMODETYPE);
         pcm.nVersion.nVersion = OMX_VERSION;
         pcm.nPortIndex = 100;
         pcm.nChannels = OUT_CHANNELS(num_channels);
         pcm.eNumData = OMX_NumericalDataSigned;
         pcm.eEndian = OMX_EndianLittle;
         pcm.nSamplingRate = sample_rate;
         pcm.bInterleaved = OMX_TRUE;
         pcm.nBitPerSample = bit_depth;
         pcm.ePCMMode = OMX_AUDIO_PCMModeLinear;

         switch(num_channels) {
         case 1:
            pcm.eChannelMapping[0] = OMX_AUDIO_ChannelCF;
            break;
         case 3:
            pcm.eChannelMapping[2] = OMX_AUDIO_ChannelCF;
            pcm.eChannelMapping[1] = OMX_AUDIO_ChannelRF;
            pcm.eChannelMapping[0] = OMX_AUDIO_ChannelLF;
            break;
         case 8:
            pcm.eChannelMapping[7] = OMX_AUDIO_ChannelRS;
         case 7:
            pcm.eChannelMapping[6] = OMX_AUDIO_ChannelLS;
         case 6:
            pcm.eChannelMapping[5] = OMX_AUDIO_ChannelRR;
         case 5:
            pcm.eChannelMapping[4] = OMX_AUDIO_ChannelLR;
         case 4:
         //pcm.eChannelMapping[3] = OMX_AUDIO_ChannelLFE;
         //YAMAHA RX-A1040 not work 8ch in original chaneelMapping.
         //Change LFE to CS,  and it work well.
            pcm.eChannelMapping[3] = OMX_AUDIO_ChannelCS;
            pcm.eChannelMapping[2] = OMX_AUDIO_ChannelCF;
         case 2:
            pcm.eChannelMapping[1] = OMX_AUDIO_ChannelRF;
            pcm.eChannelMapping[0] = OMX_AUDIO_ChannelLF;
            break;
         }

         error = OMX_SetParameter(ILC_GET_HANDLE(st->audio_render), OMX_IndexParamAudioPcm, &pcm);
         assert(error == OMX_ErrorNone);

         ilclient_change_component_state(st->audio_render, OMX_StateIdle);
         if(ilclient_enable_port_buffers(st->audio_render, 100, NULL, NULL, NULL) < 0)
         {
            // error
            ilclient_change_component_state(st->audio_render, OMX_StateLoaded);
            ilclient_cleanup_components(st->list);

            error = OMX_Deinit();
            assert(error == OMX_ErrorNone);

            ilclient_destroy(st->client);

            sem_destroy(&st->sema);
            free(st);
            *handle = NULL;
            return -1;
         }

         ilclient_change_component_state(st->audio_render, OMX_StateExecuting);
      }
   }

   return ret;
}

int32_t audioplay_delete(AUDIOPLAY_STATE_T *st)
{
   OMX_ERRORTYPE error;

   ilclient_change_component_state(st->audio_render, OMX_StateIdle);

   error = OMX_SendCommand(ILC_GET_HANDLE(st->audio_render), OMX_CommandStateSet, OMX_StateLoaded, NULL);
   assert(error == OMX_ErrorNone);

   ilclient_disable_port_buffers(st->audio_render, 100, st->user_buffer_list, NULL, NULL);
   ilclient_change_component_state(st->audio_render, OMX_StateLoaded);
   ilclient_cleanup_components(st->list);

   error = OMX_Deinit();
   assert(error == OMX_ErrorNone);

   ilclient_destroy(st->client);

   sem_destroy(&st->sema);
   free(st);

   return 0;
}

uint8_t *audioplay_get_buffer(AUDIOPLAY_STATE_T *st)
{
   OMX_BUFFERHEADERTYPE *hdr = NULL;

   hdr = ilclient_get_input_buffer(st->audio_render, 100, 0);

   if(hdr)
   {
      // put on the user list
      sem_wait(&st->sema);

      hdr->pAppPrivate = st->user_buffer_list;
      st->user_buffer_list = hdr;

      sem_post(&st->sema);
   }

   return hdr ? hdr->pBuffer : NULL;
}

int32_t audioplay_play_buffer(AUDIOPLAY_STATE_T *st,
                              uint8_t *buffer,
                              uint32_t length)
{
   OMX_BUFFERHEADERTYPE *hdr = NULL, *prev = NULL;
   int32_t ret = -1;

   if(length % st->bytes_per_sample)
      return ret;

   sem_wait(&st->sema);

   // search through user list for the right buffer header
   hdr = st->user_buffer_list;
   while(hdr != NULL && hdr->pBuffer != buffer && hdr->nAllocLen < length)
   {
      prev = hdr;
      hdr = hdr->pAppPrivate;
   }

   if(hdr) // we found it, remove from list
   {
      ret = 0;
      if(prev)
         prev->pAppPrivate = hdr->pAppPrivate;
      else
         st->user_buffer_list = hdr->pAppPrivate;
   }

   sem_post(&st->sema);

   if(hdr)
   {
      OMX_ERRORTYPE error;

      hdr->pAppPrivate = NULL;
      hdr->nOffset = 0;
      hdr->nFilledLen = length;

      error = OMX_EmptyThisBuffer(ILC_GET_HANDLE(st->audio_render), hdr);
      assert(error == OMX_ErrorNone);
   }

   return ret;
}

int32_t audioplay_set_dest(AUDIOPLAY_STATE_T *st, const char *name)
{
   int32_t success = -1;
   OMX_CONFIG_BRCMAUDIODESTINATIONTYPE ar_dest;

   if (name && strlen(name) < sizeof(ar_dest.sName))
   {
      OMX_ERRORTYPE error;
      memset(&ar_dest, 0, sizeof(ar_dest));
      ar_dest.nSize = sizeof(OMX_CONFIG_BRCMAUDIODESTINATIONTYPE);
      ar_dest.nVersion.nVersion = OMX_VERSION;
      strcpy((char *)ar_dest.sName, name);

      error = OMX_SetConfig(ILC_GET_HANDLE(st->audio_render), OMX_IndexConfigBrcmAudioDestination, &ar_dest);
      assert(error == OMX_ErrorNone);
      success = 0;
   }

   return success;
}


uint32_t audioplay_get_latency(AUDIOPLAY_STATE_T *st)
{
   OMX_PARAM_U32TYPE param;
   OMX_ERRORTYPE error;

   memset(&param, 0, sizeof(OMX_PARAM_U32TYPE));
   param.nSize = sizeof(OMX_PARAM_U32TYPE);
   param.nVersion.nVersion = OMX_VERSION;
   param.nPortIndex = 100;

   error = OMX_GetConfig(ILC_GET_HANDLE(st->audio_render), OMX_IndexConfigAudioRenderingLatency, &param);
   assert(error == OMX_ErrorNone);

   return param.nU32;
}

#define CTTW_SLEEP_TIME 10
#define MIN_LATENCY_TIME 20

void play_hdmi(int samplerate) 
{
   int bitdepth = 32;
   int nchannels = 8;
   char audio_dest[] = "hdmi";
   AUDIOPLAY_STATE_T *st;
   int32_t ret;

   //unsigned int i, j, n;
   //int phase = 0;
   //int inc = 256<<16;
   //int dinc = 0;
   int buffer_size = (BUFFER_SIZE_SAMPLES * bitdepth * OUT_CHANNELS(nchannels))>>3;

   ret = audioplay_create(&st, samplerate, nchannels, bitdepth, BN, buffer_size);
   assert(ret == 0);

   ret = audioplay_set_dest(st, audio_dest);
   assert(ret == 0);

   while (1)
   {
      uint8_t *buf;
      int32_t *p;
      //uint32_t latency;
      int ret_n;

      while((buf = audioplay_get_buffer(st)) == NULL){
         //DEBUG_PRINT("DEBUG:audioplay_get_buffer待ち");
         usleep(SLEEPTIME);
      }

      p = (int32_t *) buf;

      // fill the buffer from STDIN
      ret_n = fread(p,sizeof(int32_t),BUFFER_SIZE_SAMPLES * 8, stdin);
      if (ret_n != BUFFER_SIZE_SAMPLES * 8){
          //DEBUG_PRINT("DEBUG:%d\n",ret_n);
          for (int i = ret_n; i < BUFFER_SIZE_SAMPLES * 8; i++){
             *(p + i) = 0;
          }
      }  

      // try and wait for a minimum latency time (in ms) before
      // sending the next packet
//      while((latency = audioplay_get_latency(st)) > (samplerate * (MIN_LATENCY_TIME + CTTW_SLEEP_TIME) / 1000)){
//         DEBUG_PRINT("DEBUG:get_latency\n");
//         usleep(CTTW_SLEEP_TIME*1000);
//      }

      ret = audioplay_play_buffer(st, buf, buffer_size);
      assert(ret == 0);
   }

   audioplay_delete(st);
}

void play_hdmi4(int samplerate) 
{
   int bitdepth = 32;
   int nchannels = 8;
   char audio_dest[] = "hdmi";
   AUDIOPLAY_STATE_T *st;
   int32_t ret;
   int32_t INBUF[BUFFER_SIZE_SAMPLES][8]={};
   int buffer_size = (BUFFER_SIZE_SAMPLES * bitdepth * OUT_CHANNELS(nchannels))>>3;

   ret = audioplay_create(&st, samplerate, nchannels, bitdepth, BN, buffer_size);
   assert(ret == 0);

   ret = audioplay_set_dest(st, audio_dest);
   assert(ret == 0);

   while (1)
   {
      uint8_t *buf;
      int32_t *p;
      int32_t *p2;
      p2 = (int32_t *)INBUF;
      //uint32_t latency;
      int ret_n;

      while((buf = audioplay_get_buffer(st)) == NULL){
         //DEBUG_PRINT("DEBUG:audioplay_get_buffer待ち");
         usleep(SLEEPTIME);
      }

      p = (int32_t *) buf;

      // fill the buffer from STDIN
      //ret_n = fread(p,sizeof(int32_t),BUFFER_SIZE_SAMPLES * 8, stdin);
      ret_n = fread(p2,sizeof(int32_t),BUFFER_SIZE_SAMPLES * 8, stdin);
      if (ret_n != BUFFER_SIZE_SAMPLES * 8){
          //DEBUG_PRINT("DEBUG:%d\n",ret_n);
          for (int i = ret_n; i < BUFFER_SIZE_SAMPLES * 8; i++){
             *(p2 + i) = 0;
          }
      }  
      for (int i = 0; i < BUFFER_SIZE_SAMPLES; i++){
         *(p + 8*i + 0) = *(p2 + 8*i + 0);    
         *(p + 8*i + 1) = *(p2 + 8*i + 1);    
         *(p + 8*i + 2) = *(p2 + 8*i + 6);    
         *(p + 8*i + 3) = *(p2 + 8*i + 7);    
         *(p + 8*i + 4) = *(p2 + 8*i + 3);    
         *(p + 8*i + 5) = *(p2 + 8*i + 2);    
         *(p + 8*i + 6) = *(p2 + 8*i + 4);    
         *(p + 8*i + 7) = *(p2 + 8*i + 5);    
      }

      ret = audioplay_play_buffer(st, buf, buffer_size);
      assert(ret == 0);
   }

   audioplay_delete(st);
}

void printhelp(void)
{
  char s[]="\n"
    "This program receive 8ch 32bit data from stdin and output to HDMI\n"
    "(Raspberry Pi)  "
    "Using OpenMAX IL, not ALSA\n\n"
    "Usage: hdmi_play.bin rate\n\n"
    "Example: sox test.wav -t .s16 - | brutefir 3way.conf | hdmi_play.bin 44100\n\n";

  fprintf(stderr, "%s", s);
  return;
}

int chk_pi4(void)
{
  // 判別不能 -1
  // pi3 30
  // pi3+ 31
  // pi4 40
  // pi2 20
  // pi0  0
  // その他 -2
  FILE *fp;
  char buf[N];

  if ((fp = fopen("/proc/device-tree/model","r")) == NULL){
      return -1;
  }
  fgets(buf, N, fp);
  fclose(fp);

  if ((strstr(buf, "Pi 4 ")) != NULL){
    return 40;
  }
  if ((strstr(buf, "Pi 3 Model B Plus")) != NULL){
    return 31;
  }
  if ((strstr(buf, "Pi 3 ")) != NULL){
    return 30;
  }
  if ((strstr(buf, "Pi 2 ")) != NULL){
    return 20;
  }
  if ((strstr(buf, "Pi Zero W ")) != NULL){
    return 0;
  }
  return -2;
}



int main (int argc, char **argv)
{
   int samplerate;
   if (argc != 2){
      printhelp();
      return 1;
   }

   int pi_ver;
   pi_ver = chk_pi4();

   void (*funcp)(int); 

   if (pi_ver < 0){
     return 1;
   } else if (pi_ver == 40) {
     funcp = play_hdmi4;
   } else {
     funcp = play_hdmi;
   }

   samplerate = atoi(argv[1]);
   bcm_host_init();
   //play_hdmi(samplerate);
   funcp(samplerate);
   return 0;
}

