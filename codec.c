///
/// @file codec.c   @brief Codec functions
///
/// Copyright (c) 2021 by Jojo61.  All Rights Reserved.
///
/// Contributor(s):
///
/// License: AGPLv3
///
/// This program is free software: you can redistribute it and/or modify
/// it under the terms of the GNU Affero General Public License as
/// published by the Free Software Foundation, either version 3 of the
/// License.
///
/// This program is distributed in the hope that it will be useful,
/// but WITHOUT ANY WARRANTY; without even the implied warranty of
/// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
/// GNU Affero General Public License for more details.
///
/// $Id: d285eb28485bea02cd205fc8be47320dfe0376cf $
//////////////////////////////////////////////////////////////////////////////

///
/// @defgroup Codec The codec module.
///
/// This module contains all decoder and codec functions.
/// It is uses ffmpeg (http://ffmpeg.org) as backend.
///
/// It may work with libav (http://libav.org), but the tests show
/// many bugs and incompatiblity in it.  Don't use this shit.
///

/// compile with pass-through support (stable, AC-3, E-AC-3 only)
#define USE_PASSTHROUGH
/// compile audio drift correction support (very experimental)
#define USE_AUDIO_DRIFT_CORRECTION
/// compile AC-3 audio drift correction support (very experimental)
#define USE_AC3_DRIFT_CORRECTION
/// use ffmpeg libswresample API (autodected, Makefile)
#define noUSE_SWRESAMPLE
/// use libav libavresample API (autodected, Makefile)
#define noUSE_AVRESAMPLE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef __FreeBSD__
#include <sys/endian.h>
#else
#include <endian.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libintl.h>
#define _(str) gettext(str)             ///< gettext shortcut
#define _N(str) str                     ///< gettext_noop shortcut

#include <alsa/asoundlib.h>

#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/mem.h>

#ifdef USE_SWRESAMPLE
#include <libswresample/swresample.h>
#endif
#ifdef USE_AVRESAMPLE
#include <libavresample/avresample.h>
#include <libavutil/opt.h>
#endif

#ifndef __USE_GNU
#define __USE_GNU
#endif
#include <pthread.h>

#include "amports/aformat.h"
#include "amports/amstream.h"
#include "iatomic.h"
#include "misc.h"
#include "video.h"
#include "audio.h"
#include "codec.h"

//----------------------------------------------------------------------------
//  Global
//----------------------------------------------------------------------------
int UseAudioSpdif=0;
///
///   ffmpeg lock mutex
///
///   new ffmpeg dislikes simultanous open/close
///   this breaks our code, until this is fixed use lock.
///
static pthread_mutex_t CodecLockMutex;




#if 0

/**
**  Display pts...
**
**  ffmpeg-0.9 pts always AV_NOPTS_VALUE
**  ffmpeg-0.9 pkt_pts nice monotonic (only with HD)
**  ffmpeg-0.9 pkt_dts wild jumping -160 - 340 ms
**
**  libav 0.8_pre20111116 pts always AV_NOPTS_VALUE
**  libav 0.8_pre20111116 pkt_pts always 0 (could be fixed?)
**  libav 0.8_pre20111116 pkt_dts wild jumping -160 - 340 ms
*/
void DisplayPts(AVCodecContext * video_ctx, AVFrame * frame)
{
    int ms_delay;
    int64_t pts;
    static int64_t last_pts;

    pts = frame->pkt_pts;
    if (pts == (int64_t) AV_NOPTS_VALUE) {
        printf("*");
    }
    ms_delay = (1000 * video_ctx->time_base.num) / video_ctx->time_base.den;
    ms_delay += frame->repeat_pict * ms_delay / 2;
    printf("codec: PTS %s%s %" PRId64 " %d %d/%d %d/%d  %dms\n", frame->repeat_pict ? "r" : " ",
        frame->interlaced_frame ? "I" : " ", pts, (int)(pts - last_pts) / 90, video_ctx->time_base.num,
        video_ctx->time_base.den, video_ctx->framerate.num, video_ctx->framerate.den, ms_delay);

    if (pts != (int64_t) AV_NOPTS_VALUE) {
        last_pts = pts;
    }
}

#endif

#ifdef USE_PASSTHROUGH
///
/// Pass-through flags: CodecPCM, CodecAC3, CodecEAC3, ...
///
char CodecPassthrough;
#else
static const int CodecPassthrough = 0;
#endif
//----------------------------------------------------------------------------
//  Audio
//----------------------------------------------------------------------------



///
/// Audio decoder structure.
///
struct _audio_decoder_
{
    AVCodec *AudioCodec;                ///< audio codec
    AVCodecContext *AudioCtx;           ///< audio codec context

    char Passthrough;                   ///< current pass-through flags
    int SampleRate;                     ///< current stream sample rate
    int Channels;                       ///< current stream channels

    int HwSampleRate;                   ///< hw sample rate
    int HwChannels;                     ///< hw channels

    AVFrame *Frame;                     ///< decoded audio frame buffer

#ifdef USE_SWRESAMPLE
#if LIBSWRESAMPLE_VERSION_INT < AV_VERSION_INT(0, 15, 100)
    struct SwrContext *Resample;        ///< ffmpeg software resample context
#else
    SwrContext *Resample;               ///< ffmpeg software resample context
#endif
#endif
#ifdef USE_AVRESAMPLE
    AVAudioResampleContext *Resample;   ///< libav software resample context
#endif

    uint16_t Spdif[24576 / 2];          ///< SPDIF output buffer
    int SpdifIndex;                     ///< index into SPDIF output buffer
    int SpdifCount;                     ///< SPDIF repeat counter

    int64_t LastDelay;                  ///< last delay
    struct timespec LastTime;           ///< last time
    int64_t LastPTS;                    ///< last PTS

    int Drift;                          ///< accumulated audio drift
    int DriftCorr;                      ///< audio drift correction value
    int DriftFrac;                      ///< audio drift fraction for ac3
    int handle;                         /// Audio Device Handle
};

int AHandle;

///
/// IEC Data type enumeration.
///
enum IEC61937
{
    IEC61937_AC3 = 0x01,                ///< AC-3 data
    // FIXME: more data types
    IEC61937_EAC3 = 0x15,               ///< E-AC-3 data
};

#ifdef USE_AUDIO_DRIFT_CORRECTION
#define CORRECT_PCM 1                   ///< do PCM audio-drift correction
#define CORRECT_AC3 2                   ///< do AC-3 audio-drift correction
static char CodecAudioDrift;            ///< flag: enable audio-drift correction
#else
static const int CodecAudioDrift = 0;
#endif

static char CodecDownmix;               ///< enable AC-3 decoder downmix


/**
**  Allocate a new audio decoder context.
**
**  @returns private decoder pointer for audio decoder.
*/
AudioDecoder *CodecAudioNewDecoder(void)
{
    AudioDecoder *audio_decoder;

    if (!(audio_decoder = calloc(1, sizeof(*audio_decoder)))) {
        Fatal(_("codec: can't allocate audio decoder\n"));
    }
    if (!(audio_decoder->Frame = av_frame_alloc())) {
        Fatal(_("codec: can't allocate audio decoder frame buffer\n"));
    }

    return audio_decoder;
}

/**
**  Deallocate an audio decoder context.
**
**  @param decoder  private audio decoder
*/
void CodecAudioDelDecoder(AudioDecoder * decoder)
{
    av_frame_free(&decoder->Frame);     // callee does checks
    free(decoder);
}

/**
**  Open audio decoder.
**
**  @param audio_decoder    private audio decoder
**  @param codec_id audio   codec id
*/
extern int amlSetInt(char *, int );
void CodecAudioOpen(AudioDecoder * audio_decoder, int codec_id)
{
    AVCodec *audio_codec;
    
    if (myKernel == 4 ) {
        if (CodecPassthrough) {
            switch (codec_id) {
            case AV_CODEC_ID_MP2:
                    amlSetInt("/sys/class/audiodsp/digital_codec", 0);
                break;
            case AV_CODEC_ID_AC3:
                    amlSetInt("/sys/class/audiodsp/digital_codec", 2);
                break;
            case AV_CODEC_ID_EAC3:
                    amlSetInt("/sys/class/audiodsp/digital_codec", 4);
                break;
            case AV_CODEC_ID_AAC_LATM:
                    amlSetInt("/sys/class/audiodsp/digital_codec", 0);
                break;
            case AV_CODEC_ID_AAC:
                    amlSetInt("/sys/class/audiodsp/digital_codec", 0);
                break;
            default:
                Debug(3,"Unknown Audio Codec\n");
                return;
            }
        }
        else {
            amlSetInt("/sys/class/audiodsp/digital_codec", 0);
        }
        amlSetInt("/sys/class/audiodsp/digital_raw",CodecPassthrough ? 2: 0);
    }
#if 0  
    AHandle = audio_decoder->handle = open("/dev/amstream_abuf", O_WRONLY);
    if (audio_decoder->handle < 0)
	{	
		Debug(3,"AmlAudio open failed. %d\n",audio_decoder->handle);
        return;
	}
    int r = codec_h_ioctl_set(audio_decoder->handle, AMSTREAM_SET_AFORMAT, aFormat);
    if (r < 0) {
        Debug(3,"AmlAudio unable to set Audio Codec %d\n",aFormat);
        return;
    }
    r = codec_h_ioctl_set(audio_decoder->handle,AMSTREAM_PORT_INIT,0);
    if (r < 0) {
        Debug(3,"AmlAudio unable to Init PORT \n");
        return;
    }
    r = codec_h_ioctl_set(audio_decoder->handle, AMSTREAM_SET_ACHANNEL, 2);
    if (r < 0) {
        Debug(3,"AmlAudio unable to set Audio Channels to 2\n");
        return;
    }
    r = codec_h_ioctl_set(audio_decoder->handle, AMSTREAM_SET_SAMPLERATE, 48000);
    if (r < 0) {
        Debug(3,"AmlAudio unable to set Audio Samplerate\n");
        return;
    }
    r = codec_h_ioctl_set(audio_decoder->handle, AMSTREAM_SET_DATAWIDTH, 16);
    if (r < 0) {
        Debug(3,"AmlAudio unable to set Audio Datawidth\n");
        return;
    }
    
    //return;
#endif

    Debug(3, "codec: using audio codec ID %#06x (%s)\n", codec_id, avcodec_get_name(codec_id));

    if (!(audio_codec = avcodec_find_decoder(codec_id))) {
        // if (!(audio_codec = avcodec_find_decoder(codec_id))) {
        Fatal(_("codec: codec ID %#06x not found\n"), codec_id);
        // FIXME: errors aren't fatal
    }
    audio_decoder->AudioCodec = audio_codec;

    if (!(audio_decoder->AudioCtx = avcodec_alloc_context3(audio_codec))) {
        Fatal(_("codec: can't allocate audio codec context\n"));
    }
    
    pthread_mutex_lock(&CodecLockMutex);
    // open codec
    if (1) {
        AVDictionary *av_dict;

        av_dict = NULL;
        // FIXME: import settings
        // av_dict_set(&av_dict, "dmix_mode", "0", 0);
        // av_dict_set(&av_dict, "ltrt_cmixlev", "1.414", 0);
        // av_dict_set(&av_dict, "loro_cmixlev", "1.414", 0);
        if (avcodec_open2(audio_decoder->AudioCtx, audio_codec, &av_dict) < 0) {
            pthread_mutex_unlock(&CodecLockMutex);
            Fatal(_("codec: can't open audio codec\n"));
        }
        av_dict_free(&av_dict);
    }
    pthread_mutex_unlock(&CodecLockMutex);
    Debug(3, "codec: audio '%s'\n", audio_decoder->AudioCodec->long_name);

    audio_decoder->SampleRate = 0;
    audio_decoder->Channels = 0;
    audio_decoder->HwSampleRate = 0;
    audio_decoder->HwChannels = 0;
    audio_decoder->LastDelay = 0;

    av_log_set_level(0);
}

/**
**  Close audio decoder.
**
**  @param audio_decoder    private audio decoder
*/
void CodecAudioClose(AudioDecoder * audio_decoder)
{
    // FIXME: output any buffered data

#if 0
    if (audio_decoder->handle > 0) {
        close(audio_decoder->handle);
        audio_decoder->handle = -1;
    }
#endif

#ifdef USE_SWRESAMPLE
    if (audio_decoder->Resample) {
        swr_free(&audio_decoder->Resample);
    }
#endif
#ifdef USE_AVRESAMPLE
    if (audio_decoder->Resample) {
        avresample_free(&audio_decoder->Resample);
    }
#endif
    if (audio_decoder->AudioCtx) {
        pthread_mutex_lock(&CodecLockMutex);
        avcodec_close(audio_decoder->AudioCtx);
        av_freep(&audio_decoder->AudioCtx);
        pthread_mutex_unlock(&CodecLockMutex);
    }
}

/**
**  Set audio drift correction.
**
**  @param mask enable mask (PCM, AC-3)
*/
void CodecSetAudioDrift(int mask)
{
#ifdef USE_AUDIO_DRIFT_CORRECTION
    CodecAudioDrift = mask & (CORRECT_PCM | CORRECT_AC3);
#endif
    (void)mask;
}

/**
**  Set audio pass-through.
**
**  @param mask enable mask (PCM, AC-3, E-AC-3)
*/
void CodecSetAudioPassthrough(int mask)
{
#ifdef USE_PASSTHROUGH
    CodecPassthrough = mask & (CodecPCM | CodecAC3 | CodecEAC3);
#endif
    (void)mask;
}

/**
**  Set audio downmix.
**
**  @param onoff    enable/disable downmix.
*/
void CodecSetAudioDownmix(int onoff)
{
    if (onoff == -1) {
        CodecDownmix ^= 1;
        return;
    }
    CodecDownmix = onoff;
}

/**
**  Reorder audio frame.
**
**  ffmpeg L  R  C  Ls Rs       -> alsa L R  Ls Rs C
**  ffmpeg L  R  C  LFE Ls Rs   -> alsa L R  Ls Rs C  LFE
**  ffmpeg L  R  C  LFE Ls Rs Rl Rr -> alsa L R  Ls Rs C  LFE Rl Rr
**
**  @param buf[IN,OUT]  sample buffer
**  @param size         size of sample buffer in bytes
**  @param channels     number of channels interleaved in sample buffer
*/
static void CodecReorderAudioFrame(int16_t * buf, int size, int channels)
{
    int i;
    int c;
    int ls;
    int rs;
    int lfe;

    switch (channels) {
        case 5:
            size /= 2;
            for (i = 0; i < size; i += 5) {
                c = buf[i + 2];
                ls = buf[i + 3];
                rs = buf[i + 4];
                buf[i + 2] = ls;
                buf[i + 3] = rs;
                buf[i + 4] = c;
            }
            break;
        case 6:
            size /= 2;
            for (i = 0; i < size; i += 6) {
                c = buf[i + 2];
                lfe = buf[i + 3];
                ls = buf[i + 4];
                rs = buf[i + 5];
                buf[i + 2] = lfe; // ls
                buf[i + 3] = c;   // rs;
                buf[i + 4] = ls;  //c;
                buf[i + 5] = rs;  //lfe;

            }
            break;
        case 8:
            size /= 2;
            for (i = 0; i < size; i += 8) {
                c = buf[i + 2];
                lfe = buf[i + 3];
                ls = buf[i + 4];
                rs = buf[i + 5];
                buf[i + 2] = lfe; //ls;
                buf[i + 3] = c;   //rs;
                buf[i + 4] = ls;  //c;
                buf[i + 5] = rs;  //lfe;
            }
            break;
    }
}

void amlSetMixer(int codec) {
    
  int err;
  int cardNr = 0;
  
  if (cardNr >= 0)
  {
    snd_mixer_t *handle = NULL;
    snd_mixer_elem_t *elem;
    snd_mixer_selem_id_t *sid;

    char card[64] = { 0 };
    sprintf(card, "hw:%i", cardNr);

    Debug(3,"CAESinkALSA - Use card \"%s\" and set codec format %d\n", card,codec );

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);

    if ((err = snd_mixer_open(&handle, 0)) < 0)
    {
      Debug(3, "CAESinkALSA- can not open Mixer: %s\n", snd_strerror(err));
      return;
    }

    if ((err = snd_mixer_attach(handle, card)) < 0)
    {
      Debug(3, "CAESinkALSA - Mixer attach %s error: %s", card, snd_strerror(err));
      snd_mixer_close(handle);
      return;
    }

    if ((err = snd_mixer_selem_register(handle, NULL, NULL)) < 0)
    {
      Debug(3, "CAESinkALSA - Mixer register error: %s", snd_strerror(err));
      snd_mixer_close(handle);
      return;
    }

    if ((err = snd_mixer_load(handle)) < 0)
    {
      Debug(3, "CAESinkALSA- Mixer %s load error: %s", card, snd_strerror(err));
      snd_mixer_close(handle);
      return;
    }
    
    if (myKernel == 5) {
        Debug(3,"CAESinkALSA - Set HDMITX Source to spdif_b \n");
        snd_mixer_selem_id_set_name(sid, "HDMITX Audio Source Select");
        elem = snd_mixer_find_selem(handle, sid);
        if (!elem) {
            Debug(3, "CAESinkALSA - Unable to find simple control '%s',%i\n",
                snd_mixer_selem_id_get_name(sid), snd_mixer_selem_id_get_index(sid));
            snd_mixer_close(handle);
            return;
        } else
        snd_mixer_selem_set_enum_item(elem, (snd_mixer_selem_channel_id_t)0, UseAudioSpdif ? 0 : 1 ); // 0 = spdif  1= spdif_b
        Debug(3,"CAESinkALSA - Set SPDIF CLK Fine Setting \n");
        snd_mixer_selem_id_set_name(sid, "SPDIF CLK Fine Setting");
        elem = snd_mixer_find_selem(handle, sid);
        if (!elem) {
            Debug(3, "CAESinkALSA - Unable to find simple control '%s',%i\n",
                snd_mixer_selem_id_get_name(sid), snd_mixer_selem_id_get_index(sid));
            snd_mixer_close(handle);
            return;
        } else
        snd_mixer_selem_set_enum_item(elem, (snd_mixer_selem_channel_id_t)0, 0); // SPDIF CLK FINE Setting to 0
    }

    // set codec format for SPDIF-B
    Debug(3,"CAESinkALSA - Set codec for Spdif_b\n");
    snd_mixer_selem_id_set_name(sid, "Audio spdif_b format");
    elem = snd_mixer_find_selem(handle, sid);
    if (!elem) {
        Debug(3, "CAESinkALSA - Unable to find simple control '%s',%i\n",
            snd_mixer_selem_id_get_name(sid), snd_mixer_selem_id_get_index(sid));
        snd_mixer_close(handle);
        return;
    }

    snd_mixer_selem_set_enum_item(elem, (snd_mixer_selem_channel_id_t)0, codec);

    /* FALLTHROUGH */

    // set codec format for SPDIF-A
    Debug(3,"CAESinkALSA - Set codec for spdif\n");
    snd_mixer_selem_id_set_name(sid, "Audio spdif format");
    elem = snd_mixer_find_selem(handle, sid);
    if (!elem) {
        Debug(3, "CAESinkALSA - Unable to find simple control '%s',%i\n",
            snd_mixer_selem_id_get_name(sid), snd_mixer_selem_id_get_index(sid));
        snd_mixer_close(handle);
        return;
    }

    snd_mixer_selem_set_enum_item(elem, (snd_mixer_selem_channel_id_t)0, codec);
    snd_mixer_close(handle);
    
    
    
  }

}

/**
**  Handle audio format changes helper.
**
**  @param audio_decoder    audio decoder data
**  @param[out] passthrough pass-through output
*/
static int CodecAudioUpdateHelper(AudioDecoder * audio_decoder, int *passthrough)
{
    const AVCodecContext *audio_ctx;
    int err;

    audio_ctx = audio_decoder->AudioCtx;
    Debug(3, "codec/audio:Chanlayout %lx  Downmix %d format change %s %dHz *%d Codec ID %d channels%s%s%s%s%s\n",audio_ctx->channel_layout,CodecDownmix,
        av_get_sample_fmt_name(audio_ctx->sample_fmt), audio_ctx->sample_rate, audio_ctx->channels,
        audio_ctx->codec_id,
        CodecPassthrough & CodecPCM ? " PCM" : "", CodecPassthrough & CodecMPA ? " MPA" : "",
        CodecPassthrough & CodecAC3 ? " AC-3" : "", CodecPassthrough & CodecEAC3 ? " E-AC-3" : "",
        CodecPassthrough ? " pass-through" : "");

    *passthrough = 0;
    audio_decoder->SampleRate = audio_ctx->sample_rate;
    audio_decoder->HwSampleRate = audio_ctx->sample_rate < 44100 ? 48000 : audio_ctx->sample_rate;
    audio_decoder->Channels = audio_ctx->channels;
    audio_decoder->HwChannels = CodecDownmix ? 2 : audio_ctx->channels;
    audio_decoder->Passthrough = CodecPassthrough;

    // SPDIF/HDMI pass-through
    if ((CodecPassthrough & CodecAC3 && audio_ctx->codec_id == AV_CODEC_ID_AC3)
        || (CodecPassthrough & CodecEAC3 && audio_ctx->codec_id == AV_CODEC_ID_EAC3)) {
        if (audio_ctx->codec_id == AV_CODEC_ID_EAC3) {
            // E-AC-3 over HDMI some receivers need HBR
            audio_decoder->HwSampleRate *= 4;
        }
        audio_decoder->HwChannels = 2;
        audio_decoder->SpdifIndex = 0;  // reset buffer
        audio_decoder->SpdifCount = 0;
        *passthrough = 1;
    }
    
    
    amlSetMixer(audio_decoder->HwChannels > 2 ? 6 : 0);

    if (!*passthrough && myKernel==4)
        amlSetInt("/sys/class/audiodsp/digital_codec",audio_decoder->HwChannels > 2 ? 6 : 0 );
    
    // channels/sample-rate not support?
    if ((err = AudioSetup(&audio_decoder->HwSampleRate, &audio_decoder->HwChannels, *passthrough))) {
        // try E-AC-3 none HBR
        audio_decoder->HwSampleRate /= 4;
        if (audio_ctx->codec_id != AV_CODEC_ID_EAC3
            || (err = AudioSetup(&audio_decoder->HwSampleRate, &audio_decoder->HwChannels, *passthrough))) {

            Debug(3, "codec/audio: audio setup error\n");
            // FIXME: handle errors
            audio_decoder->HwChannels = 0;
            audio_decoder->HwSampleRate = 0;
            return err;
        }
    }

    Debug(3, "codec/audio: resample %s %dHz *%d -> %s %dHz *%d\n", av_get_sample_fmt_name(audio_ctx->sample_fmt),
        audio_ctx->sample_rate, audio_ctx->channels, av_get_sample_fmt_name(AV_SAMPLE_FMT_S16),
        audio_decoder->HwSampleRate, audio_decoder->HwChannels);

    return 0;
}

/**
**  Audio pass-through decoder helper.
**
**  @param audio_decoder    audio decoder data
**  @param avpkt            undecoded audio packet
*/
int CodecAudioPassthroughHelper(AudioDecoder * audio_decoder, const AVPacket * avpkt)
{
#ifdef USE_PASSTHROUGH
    const AVCodecContext *audio_ctx;

    audio_ctx = audio_decoder->AudioCtx;
    // SPDIF/HDMI passthrough
    if (CodecPassthrough & CodecAC3 && audio_ctx->codec_id == AV_CODEC_ID_AC3) {
        uint16_t *spdif;
        int spdif_sz;

        spdif = audio_decoder->Spdif;
        spdif_sz = 6144;

#ifdef USE_AC3_DRIFT_CORRECTION
        // FIXME: this works with some TVs/AVReceivers
        // FIXME: write burst size drift correction, which should work with all
        if (CodecAudioDrift & CORRECT_AC3) {
            int x;

            x = (audio_decoder->DriftFrac +
                (audio_decoder->DriftCorr * spdif_sz)) / (10 * audio_decoder->HwSampleRate * 100);
            audio_decoder->DriftFrac =
                (audio_decoder->DriftFrac +
                (audio_decoder->DriftCorr * spdif_sz)) % (10 * audio_decoder->HwSampleRate * 100);
            // round to word border
            x *= audio_decoder->HwChannels * 4;
            if (x < -64) {              // limit correction
                x = -64;
            } else if (x > 64) {
                x = 64;
            }
            spdif_sz += x;
        }
#endif

        // build SPDIF header and append A52 audio to it
        // avpkt is the original data
        if (spdif_sz < avpkt->size + 8) {
            Error(_("codec/audio: decoded data smaller than encoded\n"));
            return -1;
        }
        spdif[0] = htole16(0xF872);     // iec 61937 sync word
        spdif[1] = htole16(0x4E1F);
        spdif[2] = htole16(IEC61937_AC3 | (avpkt->data[5] & 0x07) << 8);
        spdif[3] = htole16(avpkt->size * 8);
        // copy original data for output
        // FIXME: not 100% sure, if endian is correct on not intel hardware
        swab(avpkt->data, spdif + 4, avpkt->size);
        // FIXME: don't need to clear always
        memset(spdif + 4 + avpkt->size / 2, 0, spdif_sz - 8 - avpkt->size);
        // don't play with the ac-3 samples
        AudioEnqueue(spdif, spdif_sz);
        return 1;
    }
    if (CodecPassthrough & CodecEAC3 && audio_ctx->codec_id == AV_CODEC_ID_EAC3) {
        uint16_t *spdif;
        int spdif_sz;
        int repeat;

        // build SPDIF header and append A52 audio to it
        // avpkt is the original data
        spdif = audio_decoder->Spdif;
        spdif_sz = 24576;               // 4 * 6144
        if (audio_decoder->HwSampleRate == 48000) {
            spdif_sz = 6144;
        }
        if (spdif_sz < audio_decoder->SpdifIndex + avpkt->size + 8) {
            Error(_("codec/audio: decoded data smaller than encoded\n"));
            return -1;
        }
        // check if we must pack multiple packets
        repeat = 1;
        if ((avpkt->data[4] & 0xc0) != 0xc0) {  // fscod
            static const uint8_t eac3_repeat[4] = { 6, 3, 2, 1 };

            // fscod2
            repeat = eac3_repeat[(avpkt->data[4] & 0x30) >> 4];
        }
        // fprintf(stderr, "repeat %d %d\n", repeat, avpkt->size);

        // copy original data for output
        // pack upto repeat EAC-3 pakets into one IEC 61937 burst
        // FIXME: not 100% sure, if endian is correct on not intel hardware
        swab(avpkt->data, spdif + 4 + audio_decoder->SpdifIndex, avpkt->size);
        audio_decoder->SpdifIndex += avpkt->size;
        if (++audio_decoder->SpdifCount < repeat) {
            return 1;
        }

        spdif[0] = htole16(0xF872);     // iec 61937 sync word
        spdif[1] = htole16(0x4E1F);
        spdif[2] = htole16(IEC61937_EAC3);
        spdif[3] = htole16(audio_decoder->SpdifIndex * 8);
        memset(spdif + 4 + audio_decoder->SpdifIndex / 2, 0, spdif_sz - 8 - audio_decoder->SpdifIndex);

        // don't play with the eac-3 samples
        AudioEnqueue(spdif, spdif_sz);

        audio_decoder->SpdifIndex = 0;
        audio_decoder->SpdifCount = 0;
        return 1;
    }
#endif
    return 0;
}

#if defined(USE_SWRESAMPLE) || defined(USE_AVRESAMPLE)

/**
**  Set/update audio pts clock.
**
**  @param audio_decoder    audio decoder data
**  @param pts              presentation timestamp
*/
static void CodecAudioSetClock(AudioDecoder * audio_decoder, int64_t pts)
{
#ifdef USE_AUDIO_DRIFT_CORRECTION
    struct timespec nowtime;
    int64_t delay;
    int64_t tim_diff;
    int64_t pts_diff;
    int drift;
    int corr;

    AudioSetClock(pts);

    delay = AudioGetDelay();
    if (!delay) {
        return;
    }
    clock_gettime(CLOCK_MONOTONIC, &nowtime);
    if (!audio_decoder->LastDelay) {
        audio_decoder->LastTime = nowtime;
        audio_decoder->LastPTS = pts;
        audio_decoder->LastDelay = delay;
        audio_decoder->Drift = 0;
        audio_decoder->DriftFrac = 0;
        Debug(4, "codec/audio: inital drift delay %" PRId64 "ms\n", delay / 90);
        return;
    }
    // collect over some time
    pts_diff = pts - audio_decoder->LastPTS;
    if (pts_diff < 10 * 1000 * 90) {
        return;
    }

    tim_diff = (nowtime.tv_sec - audio_decoder->LastTime.tv_sec)
        * 1000 * 1000 * 1000 + (nowtime.tv_nsec - audio_decoder->LastTime.tv_nsec);

    drift = (tim_diff * 90) / (1000 * 1000) - pts_diff + delay - audio_decoder->LastDelay;

    // adjust rounding error
    nowtime.tv_nsec -= nowtime.tv_nsec % (1000 * 1000 / 90);
    audio_decoder->LastTime = nowtime;
    audio_decoder->LastPTS = pts;
    audio_decoder->LastDelay = delay;

    if (0) {
        Debug(3, "codec/audio: interval P:%5" PRId64 "ms T:%5" PRId64 "ms D:%4" PRId64 "ms %f %d\n", pts_diff / 90,
            tim_diff / (1000 * 1000), delay / 90, drift / 90.0, audio_decoder->DriftCorr);
    }
    // underruns and av_resample have the same time :(((
    if (abs(drift) > 10 * 90) {
        // drift too big, pts changed?
        Debug(4, "codec/audio: drift(%6d) %3dms reset\n", audio_decoder->DriftCorr, drift / 90);
        audio_decoder->LastDelay = 0;
#ifdef DEBUG
        corr = 0;                       // keep gcc happy
#endif
    } else {

        drift += audio_decoder->Drift;
        audio_decoder->Drift = drift;
        corr = (10 * audio_decoder->HwSampleRate * drift) / (90 * 1000);
        // SPDIF/HDMI passthrough
        if ((CodecAudioDrift & CORRECT_AC3) && (!(CodecPassthrough & CodecAC3)
                || audio_decoder->AudioCtx->codec_id != AV_CODEC_ID_AC3)
            && (!(CodecPassthrough & CodecEAC3)
                || audio_decoder->AudioCtx->codec_id != AV_CODEC_ID_EAC3)) {
            audio_decoder->DriftCorr = -corr;
        }

        if (audio_decoder->DriftCorr < -20000) {    // limit correction
            audio_decoder->DriftCorr = -20000;
        } else if (audio_decoder->DriftCorr > 20000) {
            audio_decoder->DriftCorr = 20000;
        }
    }

#ifdef USE_SWRESAMPLE
    if (audio_decoder->Resample && audio_decoder->DriftCorr) {
        int distance;

        // try workaround for buggy ffmpeg 0.10
        if (abs(audio_decoder->DriftCorr) < 2000) {
            distance = (pts_diff * audio_decoder->HwSampleRate) / (900 * 1000);
        } else {
            distance = (pts_diff * audio_decoder->HwSampleRate) / (90 * 1000);
        }
        if (swr_set_compensation(audio_decoder->Resample, audio_decoder->DriftCorr / 10, distance)) {
            Debug(3, "codec/audio: swr_set_compensation failed\n");
        }
    }
#endif
#ifdef USE_AVRESAMPLE
    if (audio_decoder->Resample && audio_decoder->DriftCorr) {
        int distance;

        distance = (pts_diff * audio_decoder->HwSampleRate) / (900 * 1000);
        if (avresample_set_compensation(audio_decoder->Resample, audio_decoder->DriftCorr / 10, distance)) {
            Debug(3, "codec/audio: swr_set_compensation failed\n");
        }
    }
#endif
    if (0) {
        static int c;

        if (!(c++ % 10)) {
            Debug(3, "codec/audio: drift(%6d) %8dus %5d\n", audio_decoder->DriftCorr, drift * 1000 / 90, corr);
        }
    }
#else
    AudioSetClock(pts);
#endif
}

/**
**  Handle audio format changes.
**
**  @param audio_decoder    audio decoder data
*/
static void CodecAudioUpdateFormat(AudioDecoder * audio_decoder)
{
    int passthrough;
    const AVCodecContext *audio_ctx;

    if (CodecAudioUpdateHelper(audio_decoder, &passthrough)) {
        // FIXME: handle swresample format conversions.
        return;
    }
    if (passthrough) {                  // pass-through no conversion allowed
        return;
    }

    audio_ctx = audio_decoder->AudioCtx;

#ifdef DEBUG
    if (audio_ctx->sample_fmt == AV_SAMPLE_FMT_S16 && audio_ctx->sample_rate == audio_decoder->HwSampleRate
        && !CodecAudioDrift) {
        // FIXME: use Resample only, when it is needed!
        fprintf(stderr, "no resample needed\n");
    }
#endif

#ifndef AV_CH_LAYOUT_STEREO_DOWNMIX
#define AV_CH_LAYOUT_STEREO_DOWNMIX AV_CH_LAYOUT_STEREO
#endif 

#ifdef USE_SWRESAMPLE
#if LIBSWRESAMPLE_VERSION_INT < AV_VERSION_INT(4,5,100) 
    audio_decoder->Resample = swr_alloc_set_opts(audio_decoder->Resample, 
                                CodecDownmix ? AV_CH_LAYOUT_STEREO_DOWNMIX : audio_ctx->channel_layout , 
                                AV_SAMPLE_FMT_S16, audio_decoder->HwSampleRate,
                                audio_ctx->channel_layout, audio_ctx->sample_fmt,audio_ctx->sample_rate,
                                0, NULL);
#else
    audio_decoder->Resample = swr_alloc();
    av_opt_set_channel_layout(audio_decoder->Resample, "in_channel_layout",audio_ctx->channel_layout, 0);
    av_opt_set_channel_layout(audio_decoder->Resample, "out_channel_layout", CodecDownmix ? AV_CHANNEL_LAYOUT_STEREO_DOWNMIX : audio_ctx->channel_layout,  0);
    av_opt_set_int(audio_decoder->Resample, "in_sample_rate",     audio_ctx->sample_rate,                0);
    av_opt_set_int(audio_decoder->Resample, "out_sample_rate",    audio_decoder->HwSampleRate,                0);
    av_opt_set_sample_fmt(audio_decoder->Resample, "in_sample_fmt",  audio_ctx->sample_fmt, 0);
    av_opt_set_sample_fmt(audio_decoder->Resample, "out_sample_fmt", AV_SAMPLE_FMT_S16,  0);
#endif
    if (audio_decoder->Resample) {
	    swr_init(audio_decoder->Resample);
    } else {
	    Error(_("codec/audio: can't setup resample\n"));
    }
#endif

}
#endif
/**
**  Decode an audio packet.
**
**  PTS must be handled self.
**
**  @note the caller has not aligned avpkt and not cleared the end.
**
**  @param audio_decoder    audio decoder data
**  @param avpkt            audio packet
*/

void CodecAudioDecode(AudioDecoder * audio_decoder, const AVPacket * avpkt)
{
    AVCodecContext *audio_ctx = audio_decoder->AudioCtx;

    if (audio_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        int ret;
        AVPacket pkt[1];
        AVFrame *frame = audio_decoder->Frame;

        av_frame_unref(frame);
        *pkt = *avpkt;                  // use copy
        ret = avcodec_send_packet(audio_ctx, pkt);
        if (ret < 0) {
            Debug(3, "codec: sending audio packet failed");
            return;
        }
        ret = avcodec_receive_frame(audio_ctx, frame);
        if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
            Debug(3, "codec: receiving audio frame failed");
            return;
        }

        if (ret >= 0) {
            // update audio clock
            if (avpkt->pts != (int64_t) AV_NOPTS_VALUE) {
                CodecAudioSetClock(audio_decoder, avpkt->pts);
            }
            // format change
            if (audio_decoder->Passthrough != CodecPassthrough 
                || audio_decoder->SampleRate != audio_ctx->sample_rate
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(59,24,100)
            || audio_decoder->Channels != audio_ctx->channels) {
#else
            || audio_decoder->Channels != audio_ctx->ch_layout.nb_channels) {
#endif
                CodecAudioUpdateFormat(audio_decoder);
            }
            if (!audio_decoder->HwSampleRate || !audio_decoder->HwChannels) {
                printf("unsupported sample format\n");
                return;                 // unsupported sample format
            }
            if (CodecAudioPassthroughHelper(audio_decoder, avpkt)) {
                return;
            }
            if (audio_decoder->Resample) {
                uint8_t outbuf[8192 * 2 * 8];
                uint8_t *out[1];

                out[0] = outbuf;
                ret =
                    swr_convert(audio_decoder->Resample, out, sizeof(outbuf) / (2 * audio_decoder->HwChannels),
                    (const uint8_t **)frame->extended_data, frame->nb_samples);
                if (ret > 0) {
                    if (!(audio_decoder->Passthrough & CodecPCM)) {
                        CodecReorderAudioFrame((int16_t *) outbuf, ret * 2 * audio_decoder->HwChannels,
                            audio_decoder->HwChannels);
                    }
                    AudioEnqueue(outbuf, ret * 2 * audio_decoder->HwChannels);
                    //write(audio_decoder->handle,outbuf, ret * 2 * audio_decoder->HwChannels );
                }   
                return;
            }
        }
    }
}



/**
**  Flush the audio decoder.
**
**  @param decoder  audio decoder data
*/
void CodecAudioFlushBuffers(AudioDecoder * decoder)
{

    avcodec_flush_buffers(decoder->AudioCtx);
}
#if 0
//----------------------------------------------------------------------------
//  Codec
//----------------------------------------------------------------------------

/**
**  Empty log callback
*/
static void CodecNoopCallback( __attribute__((unused))
    void *ptr, __attribute__((unused))
    int level, __attribute__((unused))
    const char *fmt, __attribute__((unused)) va_list vl)
{
}

/**
**  Codec init
*/
void CodecInit(void)
{
    pthread_mutex_init(&CodecLockMutex, NULL);
#ifndef DEBUG
    // disable display ffmpeg error messages
    av_log_set_callback(CodecNoopCallback);
#else
    (void)CodecNoopCallback;
#endif
}

/**
**  Codec exit.
*/
void CodecExit(void)
{
    pthread_mutex_destroy(&CodecLockMutex);
}
#endif
