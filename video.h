///
/// @file video.h   @brief Video module header file
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
/// $Id: 83cd827a8744e8c80c8adba6cb87653b0ad58c45 $
//////////////////////////////////////////////////////////////////////////////

/// @addtogroup Video
/// @{

#include <libavcodec/avcodec.h>
#include "iatomic.h"


typedef enum _video_resolutions_
{
    VideoResolution576i,                ///< ...x576 interlaced
    VideoResolution720p,                ///< ...x720 progressive
    VideoResolutionFake1080i,           ///< 1280x1080 1440x1080 interlaced
    VideoResolution1080i,               ///< 1920x1080 interlaced
    VideoResolutionUHD,                 /// UHD progressive
    VideoResolutionMax                  ///< number of resolution indexs
} VideoResolutions;

/// Video output stream typedef
typedef struct __video_stream__ VideoStream;
/// Video hardware decoder typedef

typedef struct _odroid_decoder_
{


 //   xcb_window_t Window;                ///< output window

    int VideoX;                         ///< video base x coordinate
    int VideoY;                         ///< video base y coordinate
    int VideoWidth;                     ///< video base width
    int VideoHeight;                    ///< video base height

    int OutputX;                        ///< real video output x coordinate
    int OutputY;                        ///< real video output y coordinate
    int OutputWidth;                    ///< real video output width
    int OutputHeight;                   ///< real video output height

    enum AVPixelFormat PixFmt;          ///< ffmpeg frame pixfmt
    enum AVColorSpace ColorSpace;       /// ffmpeg ColorSpace
    enum AVColorTransferCharacteristic trc; //
    enum AVColorPrimaries color_primaries;
    int WrongInterlacedWarned;          ///< warning about interlace flag issued
    int Interlaced;                     ///< ffmpeg interlaced flag
    int TopFieldFirst;                  ///< ffmpeg top field displayed first

    int InputWidth;                     ///< video input width
    int InputHeight;                    ///< video input height
    AVRational InputAspect;             ///< video input aspect ratio
    VideoResolutions Resolution;        ///< resolution group

    int CropX;                          ///< video crop x
    int CropY;                          ///< video crop y
    int CropWidth;                      ///< video crop width
    int CropHeight;                     ///< video crop height

    int SurfaceField;                   ///< current displayed field
    int TrickSpeed;                     ///< current trick speed
    int TrickCounter;                   ///< current trick speed counter
    struct timespec FrameTime;          ///< time of last display
    VideoStream *Stream;                ///< video stream
    int Closing;                        ///< flag about closing current stream
    int SyncOnAudio;                    ///< flag sync to audio
    int64_t PTS;                        ///< video PTS clock

  // AVBufferRef *cached_hw_ frames_ctx;
    int LastAVDiff;                     ///< last audio - video difference
    int SyncCounter;                    ///< counter to sync frames
    int StartCounter;                   ///< counter for video start
    int FramesDuped;                    ///< number of frames duplicated
    int FramesMissed;                   ///< number of frames missed
    int FramesDropped;                  ///< number of frames dropped
    int FrameCounter;                   ///< number of frames decoded
    int FramesDisplayed;                ///< number of frames displayed
    float Frameproc;                    /// Time to process frame
    int newchannel;
} OdroidDecoder;
typedef struct _odroid_decoder_ VideoHwDecoder;
///
/// Video decoder structure.
struct _video_decoder_
{
    VideoHwDecoder *HwDecoder;          ///< video hardware decoder

    int GetFormatDone;                  ///< flag get format called!
    AVCodec *VideoCodec;                ///< video codec
    AVCodecContext *VideoCtx;           ///< video codec context
    // #ifdef FFMPEG_WORKAROUND_ARTIFACTS
    int FirstKeyFrame;                  ///< flag first frame
    // #endif
    // AVFrame *Frame;             ///< decoded video frame
    int64_t PTS;
    int filter;                         // flag for deint filter
};



//----------------------------------------------------------------------------
//  Typedefs
//----------------------------------------------------------------------------

/// Video decoder typedef.
typedef struct _video_decoder_ VideoDecoder;

/// Audio decoder typedef.
typedef struct _audio_decoder_ AudioDecoder;


#define VIDEO_PACKET_MAX 256            ///< max number of video packets  192
struct __video_stream__
{
    VideoHwDecoder *HwDecoder;          ///< video hardware decoder
    VideoDecoder *Decoder;              ///< video decoder
    pthread_mutex_t DecoderLockMutex;   ///< video decoder lock mutex

    enum AVCodecID CodecID;             ///< current codec id
    enum AVCodecID LastCodecID;         ///< last codec id

    volatile char NewStream;            ///< flag new video stream
    volatile char ClosingStream;        ///< flag closing video stream
    volatile char SkipStream;           ///< skip video stream
    volatile char Freezed;              ///< stream freezed

    volatile char TrickSpeed;           ///< current trick speed
    volatile char Close;                ///< command close video stream
    volatile char ClearBuffers;         ///< command clear video buffers
    volatile char ClearClose;           ///< clear video buffers for close

    int InvalidPesCounter;              ///< counter of invalid PES packets

    enum AVCodecID CodecIDRb[VIDEO_PACKET_MAX]; ///< codec ids in ring buffer
    AVPacket PacketRb[VIDEO_PACKET_MAX];    ///< PES packet ring buffer
    int StartCodeState;                 ///< last three bytes start code state

    int PacketWrite;                    ///< ring buffer write pointer
    int PacketRead;                     ///< ring buffer read pointer
    atomic_t PacketsFilled;             ///< how many of the ring buffer is used
};
//----------------------------------------------------------------------------
//  Typedefs
//----------------------------------------------------------------------------





//----------------------------------------------------------------------------
//  Variables
//----------------------------------------------------------------------------

extern signed char VideoHardwareDecoder;    ///< flag use hardware decoder
extern char VideoIgnoreRepeatPict;      ///< disable repeat pict warning
extern int VideoAudioDelay;             ///< audio/video delay
extern char ConfigStartX11Server;       ///< flag start the x11 server
extern char MyConfigDir[];
//----------------------------------------------------------------------------
//  Prototypes
//----------------------------------------------------------------------------

#ifdef USE_OPENGLOSD
/// Set callback funktion to notify VDR about VideoEvents
extern void VideoSetVideoEventCallback(void (*)(void));
#endif

/// Allocate new video hardware decoder.
extern VideoHwDecoder *VideoNewHwDecoder(VideoStream *);

/// Deallocate video hardware decoder.
extern void VideoDelHwDecoder(VideoHwDecoder *);

/// Poll video events.
extern void VideoPollEvent(void);

/// Wakeup display handler.
extern void VideoDisplayWakeup(void);

/// Set video device.
extern void VideoSetDevice(const char *);

/// Get video driver name.
extern const char *VideoGetDriverName(void);

/// Set video geometry.
extern int VideoSetGeometry(const char *);

/// Set 60Hz display mode.
extern void VideoSet60HzMode(int);

/// Set soft start audio/video sync.
extern void VideoSetSoftStartSync(int);

/// Set show black picture during channel switch.
extern void VideoSetBlackPicture(int);

/// Set brightness adjustment.
extern void VideoSetBrightness(int);

/// Set contrast adjustment.
extern void VideoSetContrast(int);

/// Set saturation adjustment.
extern void VideoSetSaturation(int);

/// Set Gamma.
extern void VideoSetGamma(int);

/// Set Color Temp.
extern void VideoSetTemperature(int);

/// Set ColorSpace.
extern void VideoSetTargetColor(int);

/// Set hue adjustment.
extern void VideoSetHue(int);

/// Set Color Blindness.
extern void VideoSetColorBlindness(int);

/// Set Color Blindness Faktor
extern void VideoSetColorBlindnessFaktor(int);

/// Set video output position.
extern void VideoSetOutputPosition(VideoHwDecoder *, int, int, int, int);

/// Set video mode.
extern void VideoSetVideoMode(int, int, int, int);

/// Set 4:3 display format.
extern void VideoSet4to3DisplayFormat(int);

/// Set other display format.
extern void VideoSetOtherDisplayFormat(int);

/// Set video fullscreen mode.
extern void VideoSetFullscreen(int);

/// Set deinterlace.
extern void VideoSetDeinterlace(int[]);

/// Set skip chroma deinterlace.
extern void VideoSetSkipChromaDeinterlace(int[]);

/// Set inverse telecine.
extern void VideoSetInverseTelecine(int[]);

/// Set scaling.
extern void VideoSetScaling(int[]);

/// Set scaler test.
extern void VideoSetScalerTest(int);

/// Set denoise.
extern void VideoSetDenoise(int);

/// Set Hdr 2 Sdr Mode
extern void VideoSetHdr2Sdr(int);

/// Set sharpen.
extern void VideoSetSharpen(int[]);

/// Set cut top and bottom.
extern void VideoSetCutTopBottom(int[]);

/// Set cut left and right.
extern void VideoSetCutLeftRight(int[]);

/// Set studio levels.
extern void VideoSetStudioLevels(int);

/// Set Screen Resolution
extern void VideoSetScreenResolution(int);

/// Set background.
extern void VideoSetBackground(uint32_t);

/// Set audio delay.
extern void VideoSetAudioDelay(int);

/// Clear OSD.
extern void VideoOsdClear(void);

/// Draw an OSD ARGB image.
extern void VideoOsdDrawARGB(int, int, int, int, int, const uint8_t *, int, int);

/// Get OSD size.
extern void VideoGetOsdSize(int *, int *);

/// Set OSD size.
extern void VideoSetOsdSize(int, int);

/// Set Osd 3D Mode
extern void VideoSetOsd3DMode(int);

/// Set video clock.
extern void VideoSetClock(VideoHwDecoder *, int64_t);

/// Get video clock.
extern int64_t VideoGetClock(const VideoHwDecoder *);

/// Set closing flag.
extern void VideoSetClosing(VideoHwDecoder *);

/// Reset start of frame counter
extern void VideoResetStart(VideoHwDecoder *);

/// Set trick play speed.
extern void VideoSetTrickSpeed(VideoHwDecoder *, int);

/// Grab screen.
extern uint8_t *VideoGrab(int *, int *, int *, int);

/// Grab screen raw.
extern uint8_t *VideoGrabService(int *, int *, int *);

/// Get decoder statistics.
extern void VideoGetStats(VideoHwDecoder *, int *, int *, int *, int *, float *, int *, int *, int *, int *);

/// Get video stream size
extern void VideoGetVideoSize(VideoHwDecoder *, int *, int *, int *, int *);

extern void VideoOsdInit(void);         ///< Setup osd.
extern void VideoOsdExit(void);         ///< Cleanup osd.

extern void VideoInit(const char *);    ///< Setup video module.
extern void VideoExit(void);            ///< Cleanup and exit video module.

/// Poll video input buffers.
extern int VideoPollInput(VideoStream *);

/// Decode video input buffers.
extern int VideoDecodeInput(VideoStream *);

/// Get number of input buffers.
extern int VideoGetBuffers(const VideoStream *);

/// Set DPMS at Blackscreen switch
extern void SetDPMSatBlackScreen(int);

/// Raise the frontend window
extern int VideoRaiseWindow(void);

/// Set Shaders
extern int VideoSetShader(char *);


#ifdef GAMMA
extern void Init_Gamma();
extern void Exit_Gamma();
extern void Set_Gamma(float, int);
extern void Get_Gamma();
#endif

#if 0
long int gettid()
{
    return (long int)syscall(224);
}
#endif

/// @}
