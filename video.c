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

#include <stdint.h>
#include <stddef.h>
#include "ge2d.h"
#include "ge2d_cmd.h"
#include "ion.h"
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <libavcodec/avcodec.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <linux/kd.h>
#include <X11/Xlib.h>

#include "codec_type.h"
#include "amports/amstream.h"
#define VIDEO_WIDEOPTION_NORMAL (0)
#define VIDEO_DISABLE_NONE    (0)


#include "video.h"
#include "codec.h"
#include "audio.h"
#include "misc.h"

#define false False
#define true True
#define bool Bool

struct Rectangle
{
   float X;
	float Y;
	float Width;
	float Height;
};

struct PackedColor
{
	unsigned char R;
	unsigned char G;
	unsigned char B;
	unsigned char A;
};

static OdroidDecoder *OdroidDecoders[2];  ///< open decoder streams
static struct timespec OdroidFrameTime;  ///< time of last display
static int VideoWindowX = 0;                ///< video output window x coordinate
static int VideoWindowY = 0;                ///< video outout window y coordinate
static int VideoWindowWidth = 1920;       ///< video output window width
static int VideoWindowHeight = 1080;      ///< video output window height
static int ScreenResolution;
static int OsdConfigWidth;              ///< osd configured width
static int OsdConfigHeight;             ///< osd configured height
static int OsdWidth;                    ///< osd width
static int OsdHeight;                   ///< osd height
static void (*VideoEventCallback)(void) = NULL; /// callback function to notify


static pthread_t VideoThread;           ///< video decode thread
static pthread_cond_t VideoWakeupCond;  ///< wakeup condition variable
static pthread_mutex_t VideoMutex;      ///< video condition mutex
static pthread_mutex_t VideoLockMutex;  ///< video lock mutex
pthread_mutex_t OSDMutex;               ///< OSD update mutex
static int64_t VideoDeltaPTS = 0;  

/// Default audio/video delay
int VideoAudioDelay;

enum ApiLevel
{
	UnknownApi = 0,
	S805,
	S905
};
enum VideoFormatEnum
{
	UnknownVideo = 0,
	Mpeg2,
	Mpeg4V3,
	Mpeg4,
	Avc,
	VC1,
	Hevc,
	Yuv420,
	NV12,
	NV21
};

enum AudioFormatEnum
{
	UnknownAudio = 0,
	Pcm,
	MpegLayer2,
	Mpeg2Layer3,
	Ac3,
	Aac,
	Dts,
	WmaPro,
	DolbyTrueHD,
	EAc3,
	Opus,
	Vorbis,
	PcmDvd,
	Flac,
	PcmS24LE
};

int videoFormat = 0;
int width;
int height;
double FrameRate = 25.0;
int apiLevel;
int isOpen = false;
int isRunning = false;
double lastTimeStamp = -1;
bool isFirstVideoPacket = true;
bool isAnnexB = false;
bool isShortStartCode = false;
bool isExtraDataSent = false;
int64_t estimatedNextPts = 0;
int Hdr2Sdr = 0;
int NoiseReduction = 1;

const uint64_t PTS_FREQ = 90000;

bool doPauseFlag = false;
bool doResumeFlag = false;


AVRational timeBase;

int handle,cntl_handle,fd;

const char* CODEC_VIDEO_ES_DEVICE = "/dev/amstream_vbuf";
const char* CODEC_VIDEO_ES_HEVC_DEVICE = "/dev/amstream_hevc";
const char* CODEC_CNTL_DEVICE = "/dev/amvideo";

const long EXTERNAL_PTS = (1);
const long SYNC_OUTSIDE = (2);
const long USE_IDR_FRAMERATE = 0x04;
const long UCODE_IP_ONLY_PARAM = 0x08;
const long MAX_REFER_BUF = 0x10;
const long ERROR_RECOVERY_MODE_IN = 0x20;

/// Poll video events.
 void VideoPollEvent(void) {};


/// Set video device.
 void VideoSetDevice(const char *i) {};

/// Get video driver name.
 const char *VideoGetDriverName(void) {};

/// Set video geometry.
 int VideoSetGeometry(const char *geometry) {

	XParseGeometry(geometry, &VideoWindowX, &VideoWindowY, &VideoWindowWidth, &VideoWindowHeight);

    return 0;
 };

/// Set 60Hz display mode.
 void VideoSet60HzMode(int i) {};

/// Set soft start audio/video sync.
 void VideoSetSoftStartSync(int i) {};

/// Set show black picture during channel switch.
 void VideoSetBlackPicture(int i) {};

/// Set brightness adjustment.
 void VideoSetBrightness(int i) {};

/// Set contrast adjustment.
 void VideoSetContrast(int i) {};

/// Set saturation adjustment.
 void VideoSetSaturation(int i) {};

/// Set Gamma.
 void VideoSetGamma(int i) {};

/// Set Color Temp.
 void VideoSetTemperature(int i) {};

/// Set ColorSpace.
 void VideoSetTargetColor(int i) {};

/// Set hue adjustment.
 void VideoSetHue(int i) {};

/// Set Color Blindness.
 void VideoSetColorBlindness(int i) {};

/// Set Color Blindness Faktor
 void VideoSetColorBlindnessFaktor(int i) {};

/// Set video output position.
void VideoSetOutputPosition(VideoHwDecoder *decoder, int x, int y, int width, int height) {


	if (!OsdWidth || !OsdHeight) {
		return;
	}
	if (!width || !height) {
		// restore full size
		width = VideoWindowWidth;
		height = VideoWindowHeight;
	} else {
		// convert OSD coordinates to window coordinates
		x = (x * VideoWindowWidth) / OsdWidth;
		width = (width * VideoWindowWidth) / OsdWidth;
		y = (y * VideoWindowHeight) / OsdHeight;
		height = (height * VideoWindowHeight) / OsdHeight;
	}

	amlSetVideoAxis(x,y,x+width,y+height);
};

/// Set 4 {} {};3 display format.
 void VideoSet4to3DisplayFormat(int i) {};

/// Set other display format.
 void VideoSetOtherDisplayFormat(int i) {};

/// Set video fullscreen mode.
 void VideoSetFullscreen(int i) {};

/// Set deinterlace.
 void VideoSetDeinterlace(int *i) {};

/// Set skip chroma deinterlace.
 void VideoSetSkipChromaDeinterlace(int *i) {};

/// Set inverse telecine.
 void VideoSetInverseTelecine(int *i) {};

/// Set scaling.
 void VideoSetScaling(int *i) {};

/// Set scaler test.
 void VideoSetScalerTest(int i) {};


/// Set sharpen.
 void VideoSetSharpen(int *i) {};

/// Set cut top and bottom.
 void VideoSetCutTopBottom(int *i) {};

/// Set cut left and right.
 void VideoSetCutLeftRight(int *i) {};

/// Set studio levels.
 void VideoSetStudioLevels(int i) {};

/// Set background.
 void VideoSetBackground(uint32_t i) {};

/// Set audio delay.
void VideoSetAudioDelay(int ms) {
	VideoAudioDelay = ms * 90;
 };

/// Clear OSD.
void VideoOsdClear(void) {
    ClearDisplay();
 };

 void VideoSetScreenResolution (int r) {
	 ScreenResolution = r;
 }

int64_t VideoGetClock(const VideoHwDecoder *i) {};


/// Draw an OSD ARGB image.
 void VideoOsdDrawARGB(int i, int j, int k, int l, int m, const uint8_t *n, int o, int p) {};

/// Get OSD size.
void VideoGetOsdSize(int *width, int *height) {

    *width = 1920;
    *height = 1080;

	if (OsdWidth && OsdHeight) {
        *width = OsdWidth;
        *height = OsdHeight;
    }
	

};

/// Set OSD size.
void VideoSetOsdSize(int width, int height)
{

    if (OsdConfigWidth != width || OsdConfigHeight != height) {
        VideoOsdExit();
        OsdConfigWidth = width;
        OsdConfigHeight = height;
        VideoOsdInit();
    }
}

/// Set Osd 3D Mode
 void VideoSetOsd3DMode(int i) {};

/// Set closing flag.
void VideoSetClosing(VideoHwDecoder *decoder) {
	 decoder->Closing = 1;
};

/// Set trick play speed.
void VideoSetTrickSpeed(VideoHwDecoder *decoder, int speed) {
	if (speed > 6)
		speed = 6;
	decoder->TrickSpeed = speed;
    decoder->TrickCounter = speed;
    if (speed) {
		amlFreerun(1);
        decoder->Closing = 0;
    } else {
		amlFreerun(0);
	}
};

/// Grab screen.
 uint8_t *VideoGrab(int *i, int *j, int *k, int l) {};

/// Grab screen raw.
 uint8_t *VideoGrabService(int *i, int *j, int *k) {};

/// Get decoder statistics.
 void VideoGetStats(VideoHwDecoder *i, int *j, int *k, int *l, int *m, float *n, int *o, int *p, int *q, int *r) {};

/// Get video stream size
 void VideoGetVideoSize(VideoHwDecoder *i, int *j, int *k, int *l, int *m) {};

 void VideoOsdInit(void) {

	OsdWidth = VideoWindowWidth;
	OsdHeight = VideoWindowHeight;
 	
 };         ///< Setup osd


 void VideoOsdExit(void) {};         ///< Cleanup osd.


 void VideoExit(void) {
	
	// Restore alpha setting
	int fd_m;
	struct fb_var_screeninfo info;

	fd_m = open("/dev/fb0", O_RDWR);
	ioctl(fd_m, FBIOGET_VSCREENINFO, &info);
	info.reserved[0] = 0;
	info.reserved[1] = 0;
	info.reserved[2] = 0;
	info.xoffset = 0;
	info.yoffset = 0;
	info.activate = FB_ACTIVATE_NOW;
	info.red.offset = 16;
	info.red.length = 8;
	info.green.offset = 8;
	info.green.length = 8;
	info.blue.offset = 0;
	info.blue.length = 8;
	info.transp.offset = 24;
	info.transp.length = 0;
	info.nonstd = 1;
	info.yres_virtual = info.yres * 2;
	ioctl(fd_m, FBIOPUT_VSCREENINFO, &info);
	close(fd_m);

#if 0
	char vid60hz[] = "1080p60hz";
	if (ScreenResolution) {
		fd = open("/sys/class/display/mode",O_RDWR);
		write(fd,vid60hz,sizeof(vid60hz));
		close(fd);
	}
#endif

	// Set text mode
	int ttyfd = open("/dev/tty0", O_RDWR);
	if (ttyfd < 0)
	{
		printf("Could not open /dev/tty0\n");
	}
	else
	{
		int ret = ioctl(ttyfd, KDSETMODE, KD_TEXT);
		if (ret < 0) {
			printf("KDSETMODE failed.");
			return;
		}
		close(ttyfd);
	}
 };            ///< Cleanup and exit video module.



/// Set DPMS at Blackscreen switch
 void SetDPMSatBlackScreen(int i) {};

/// Raise the frontend window
 int VideoRaiseWindow(void) {};

/// Set Shaders
 int VideoSetShader(char *i) {};


/// Deallocate a video decoder context.
 void CodecVideoDelDecoder(VideoDecoder *i){};

/// Close video codec.
 void CodecVideoClose(VideoDecoder *decoder){};

extern void amlClearVBuf();
/// Flush video buffers.
 void CodecVideoFlushBuffers(VideoDecoder *decoder) {};


/// Setup and initialize codec module.
 void CodecInit(void){};

/// Cleanup and exit codec module.
 void CodecExit(void){};

 void SetVideoMode(void){};

  /// C callback feed key press
    // void FeedKeyPress(const char *i, const char *j, int k, int l, const char *m) {};

           
     
/// C plugin play TS video packet
void PlayTsVideo(const uint8_t *i, int j) {};
   
void VideoSetHdr2Sdr(int i) 
{

	Hdr2Sdr = i;
	if (Hdr2Sdr) {
		Debug(3,"Enable HDR2SDR\n");
		amlSetInt("/sys/module/am_vecm/parameters/hdr_mode", 1);
	} else {
		Debug(3,"Disable HDR2SDR\n");
		amlSetInt("/sys/module/am_vecm/parameters/hdr_mode", 0);
	}
};

void VideoSetDenoise(int i) 
{

	NoiseReduction = i;
	if (!NoiseReduction) {
		Debug(3,"Disable Noise reduction\n");
		amlSetString("/sys/module/di/parameters/nr2_en", "0");
	} else {
		Debug(3,"Enable Noise reduction\n");
		amlSetString("/sys/module/di/parameters/nr2_en", "1");
	}
};           

void VideoDelHwDecoder(VideoHwDecoder * hw_decoder)
{
    if (hw_decoder) {

        // only called from inside the thread
        // VideoThreadLock();
//        VideoUsedModule->DelHwDecoder(hw_decoder);
        // VideoThreadUnlock();
    }
}


extern int64_t AudioGetClock(void);
extern int64_t GetCurrentPts(void);
extern void SetCurrentPts(double );
void ProcessClockBuffer()
	{
		// truncate to 32bit
		uint64_t apts;
		uint64_t pts = apts = (uint64_t)AudioGetClock();
		pts &= 0xffffffff;

		uint64_t vpts = (uint64_t)GetCurrentPts() ;
		vpts &= 0xffffffff;

		if (!pts || !vpts) {
			return;
		}

		pts = (pts + VideoAudioDelay) & 0xffffffff;

		double drift = ((double)vpts - (double)pts) / (double)PTS_FREQ;
		//double driftTime = drift / (double)PTS_FREQ;

		double driftFrames = drift * 25.0; // frameRate;
		const int maxDelta = 2;

		// To minimize clock jitter, only adjust the clock if it
		// deviates more than +/- 2 frames
		if (driftFrames >= maxDelta || driftFrames <= -maxDelta)
		{
			{
				
				SetCurrentPts(apts);

				//printf("AmlVideoSink: Adjust PTS - pts=%f vpts=%f drift=%f (%f frames)\n", pts / (double)PTS_FREQ, vpts / (double)PTS_FREQ, drift, driftFrames);
			}
		}

	}

void CodecVideoDecode(VideoDecoder * decoder, const AVPacket * avpkt) 
{	
	decoder->PTS = avpkt->pts;   		// save last pts;
	if (!decoder->HwDecoder->TrickSpeed)
		ProcessClockBuffer();
	else {
		if (avpkt->pts != AV_NOPTS_VALUE) {
			SetCurrentPts(avpkt->pts);
			//printf("push buffer ohne sync PTS %ld\n",avpkt->pts);
		}
	}
	ProcessBuffer(avpkt);
	if (decoder->HwDecoder->TrickSpeed) {
		usleep(20000*decoder->HwDecoder->TrickSpeed);
	}
	//amlGetBufferStatus();
};


void ClearDisplay(void)
{
	int io;
   extern int ge2d_fd;

	// Configure src/dst
    struct config_para_ex_ion_s fill_config = { 0 };
    fill_config.alu_const_color = 0xffffffff;

    fill_config.src_para.mem_type = CANVAS_OSD0;
    fill_config.src_para.format = GE2D_FORMAT_S32_ARGB;
    fill_config.src_para.left = 0;
    fill_config.src_para.top = 0;
    fill_config.src_para.width = OsdWidth; //width;
    fill_config.src_para.height = OsdHeight; //height;
    fill_config.src_para.x_rev = 0;
    fill_config.src_para.y_rev = 0;

    fill_config.dst_para = fill_config.src_para;
    

    io = ioctl(ge2d_fd, GE2D_CONFIG_EX_ION, &fill_config);
    if (io < 0)
    {
        printf("GE2D_CONFIG failed. Clear Display");
    }


    // Perform the fill operation
    struct ge2d_para_s fillRect = { 0 };

    fillRect.src1_rect.x = 0;
    fillRect.src1_rect.y = 0;
    fillRect.src1_rect.w = OsdWidth; //width;
    fillRect.src1_rect.h = OsdHeight; //height;
    fillRect.color = 0;

    io = ioctl(ge2d_fd, GE2D_FILLRECTANGLE, &fillRect);
    if (io < 0)
    {
        printf("GE2D_FILLRECTANGLE failed.\n");
    }
}

#if 0
void WaitVsync() {
extern int fd;   
if (ioctl(fd, FBIO_WAITFORVSYNC, 0) < 0)
	{
		printf("FBIO_WAITFORVSYNC failed.\n");
	}
}
#endif

void ClearCursor(int blank) {
int fd = open("/dev/tty1", O_RDWR);
    
    if(0 < fd)
    {
        write(fd, "\033[?25", 5);
        write(fd, blank==0 ? "h" : "l", 1);
    }
    close(fd);
}

void Fbdev_blank(int blank) {
int fd = open("/dev/tty0", O_RDWR);
    
    if(0 < fd)
    {
        ioctl(fd, FBIOBLANK, blank ? VESA_POWERDOWN: VESA_NO_BLANKING);
    }
    close(fd);
}

///
/// Video display wakeup.
///
/// New video arrived, wakeup video thread.
///
void VideoDisplayWakeup(void)
{

    if (!VideoThread) {                 // start video thread, if needed
        VideoThreadInit();
    }
}



void *VideoDisplayHandlerThread(void *dummy)
{

  //  prctl(PR_SET_NAME, "video decoder", 0, 0, 0);
    
 //   pthread_cleanup_push(delete_decode, NULL);
    for (;;) {
        // fix dead-lock with OdroidExit
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        pthread_testcancel();
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

        OdroidDisplayHandlerThread();
    }
  //  pthread_cleanup_pop(NULL);
    return dummy;
}

//
/// Initialize video threads.
///
void VideoThreadInit(void)
{
  
 //   pthread_mutex_init(&VideoMutex, NULL);
 //   pthread_mutex_init(&VideoLockMutex, NULL);
 //   pthread_mutex_init(&OSDMutex, NULL);
 //   pthread_cond_init(&VideoWakeupCond, NULL);
    //pthread_create(&VideoThread, NULL, VideoDisplayHandlerThread, NULL);

 //   pthread_create(&VideoDisplayThread, NULL, VideoHandlerThread, NULL);

}

///
/// Handle a Odroid display.
///
void OdroidDisplayHandlerThread(void)
{
    int i;
    int err = 0;
    int allfull;
    int decoded;
    int64_t filled;
    struct timespec nowtime;
    OdroidDecoder *decoder;
	
	int64_t akt_pts, new_pts;

    allfull = 1;
    decoded = 0;


    decoder = OdroidDecoders[0];
	
	filled = 0;

    if (filled < 60) {
        // FIXME: hot polling
        // fetch+decode or reopen
        allfull = 0;
        err = VideoDecodeInput(decoder->Stream);
    } else {
        err = VideoPollInput(decoder->Stream);
    }
    // decoder can be invalid here
    if (err) {
        // nothing buffered?
        if (err == -1 && decoder->Closing) {
            decoder->Closing--;
            if (!decoder->Closing) {
                Debug(3, "video/Odroid: closing eof\n");
                decoder->Closing = -1;
            }
        }
        usleep(10 * 1000);

    }
	else
    	decoded = 1;
    

    if (!decoded) {                     // nothing decoded, sleep
        // FIXME: sleep on wakeup
        usleep(5 * 1000);
    }
    usleep(10);
	return;
    // all decoder buffers are full
    // and display is not preempted
    // speed up filling display queue, wait on display queue empty
    if (!allfull && !decoder->TrickSpeed) {
        clock_gettime(CLOCK_MONOTONIC, &nowtime);
        // time for one frame over?
        if ((nowtime.tv_sec - OdroidFrameTime.tv_sec) * 1000 * 1000 * 1000 + (nowtime.tv_nsec - OdroidFrameTime.tv_nsec) < 15 * 1000 * 1000) {
            return;
        }
    }

    return;
}

///
/// Allocate new Odroid decoder.
///
/// @param stream   video stream
///
/// @returns a new prepared Odroid hardware decoder.
///
OdroidDecoder *VideoNewHwDecoder(VideoStream * stream)
{

    OdroidDecoder *decoder;

    int i = 0;

   
    Debug(3, "Odroid New HW Decoder\n");

    
    if (!(decoder = calloc(1, sizeof(*decoder)))) {
        Debug(3,"video/Odroid: out of memory\n");
        return NULL;
    }

    // decoder->VideoX = 0;  // done by calloc
    // decoder->VideoY = 0;
    decoder->VideoWidth = VideoWindowWidth;
    decoder->VideoHeight = VideoWindowHeight;

    decoder->OutputWidth = VideoWindowWidth;
    decoder->OutputHeight = VideoWindowHeight;
    decoder->PixFmt = AV_PIX_FMT_NONE;

    decoder->Stream = stream;
    
    decoder->Closing = -300 - 1;
    decoder->PTS = AV_NOPTS_VALUE;

    OdroidDecoders[0] = decoder;

    return decoder;
}

void VideoSetVideoMode( __attribute__((unused))
    int x, __attribute__((unused))
    int y, int width, int height)
{
    Debug(4, "video: %s %dx%d%+d%+d\n", __FUNCTION__, width, height, x, y);

    if ((unsigned)width == VideoWindowWidth && (unsigned)height == VideoWindowHeight) {
        return;                         // same size nothing todo
    }

    if (VideoEventCallback) {
        sleep(1);
        VideoEventCallback();
        Debug(3, "call back set video mode %d %d\n", width, height);
    }

   
    VideoWindowWidth = width;
    VideoWindowHeight = height;

    SetVideoMode();
    

}

void VideoSetVideoEventCallback(void (*videoEventCallback)(void))
{
    VideoEventCallback = videoEventCallback;
}

VideoDecoder *CodecVideoNewDecoder(VideoHwDecoder * hw_decoder)
{
    VideoDecoder *decoder;

    if (!(decoder = calloc(1, sizeof(*decoder)))) {
        Debug(3,"codec: can't allocate video decoder\n");
    }
    decoder->HwDecoder = hw_decoder;
    decoder->PTS = AV_NOPTS_VALUE;
    return decoder;
}

void VideoResetStart(VideoHwDecoder * hw_decoder)
{

    Debug(3, "video: reset start\n");
	//amlReset();

}

void VideoSetClock(VideoHwDecoder * decoder, int64_t pts)
{
    decoder->PTS = pts;
}

int GetApiLevel()
{
	int fd = open(CODEC_VIDEO_ES_DEVICE, O_WRONLY);

    apiLevel = 0;

	if (fd < 0)
	{
		printf("AmlCodec open failed.\n");
        return;
	}


	apiLevel = S805;

	int version;
	int r = ioctl(fd, AMSTREAM_IOC_GET_VERSION, &version);
	if (r == 0)
	{
		Debug(3,"AmlCodec: amstream version : %d.%d\n", (version & 0xffff0000) >> 16, version & 0xffff);

		if (version >= 0x20000)
		{
			apiLevel = S905;
		}
	}

	close(fd);
    return;
}

 void VideoInit(const char *i) 
 {

	const char *const sr[] = {
        "auto", "1080p50hz" ,"1080p60hz" , "2160p50hz" ,"2160p60hz" , "2160p50hz420" ,"2160p60hz420" 
    };

	timeBase.num = 1;
	timeBase.den = 90000;

	if (ScreenResolution > 0 && ScreenResolution < 7) {
		fd = open("/sys/class/display/mode",O_RDWR);
		write(fd,sr[ScreenResolution],sizeof(sr[ScreenResolution])+1);
		close(fd);
	}

	if (ScreenResolution > 2) {
		OsdWidth = VideoWindowWidth = 3840;
		OsdHeight =  VideoWindowHeight =  2160;
	} else {
		OsdWidth =  VideoWindowWidth = 1920;
		OsdHeight =  VideoWindowHeight = 1080;
	}
	// enable alpha setting

	struct fb_var_screeninfo info;

	fd = open("/dev/fb0", O_RDWR);
	ioctl(fd, FBIOGET_VSCREENINFO, &info);
	info.reserved[0] = 0;
	info.reserved[1] = 0;
	info.reserved[2] = 0;
	info.xoffset = 0;
	info.yoffset = 0;
	info.activate = FB_ACTIVATE_ALL;
	info.red.offset = 16;
	info.red.length = 8;
	info.green.offset = 8;
	info.green.length = 8;
	info.blue.offset = 0;
	info.blue.length = 8;
	info.transp.offset = 24;
	info.transp.length = 8;

	if (!ScreenResolution) {
		OsdWidth =  VideoWindowWidth = info.xres;
		OsdHeight =  VideoWindowHeight = info.yres;
	}
	
	info.bits_per_pixel = 32;
	info.nonstd = 1;
	ioctl(fd, FBIOPUT_VSCREENINFO, &info);
	close(fd);

	// Set graphics mode
	int ttyfd = open("/dev/tty0", O_RDWR);	
	if (ttyfd < 0)
	{
		printf("Could not open /dev/tty0\n");
		return;
	}
	else
	{	int ret = ioctl(ttyfd, KDSETMODE, KD_GRAPHICS);
		if (ret < 0) {
			printf("KDSETMODE failed.");
			return;
		}

		close(ttyfd);
	}
	GetApiLevel();
	Debug(3,"aml ApiLevel = %d\n",apiLevel);
 };    
 
///< Setup video module.
// Open video codec.
 void CodecVideoOpen(VideoDecoder *decoder, int codec_id, AVPacket *avpkt)
 {

	switch (codec_id)
	{
	case AV_CODEC_ID_MPEG2VIDEO:
			videoFormat = Mpeg2;
		break;

	case AV_CODEC_ID_MSMPEG4V3:
		
			videoFormat = Mpeg4V3;
		break;

	case AV_CODEC_ID_MPEG4:
		
			videoFormat = Mpeg4;
		break;

	case AV_CODEC_ID_H264:
		
			videoFormat = Avc;

		break;

	case AV_CODEC_ID_HEVC:
		
			videoFormat = Hevc;
		break;

	case AV_CODEC_ID_VC1:
		
			videoFormat = VC1;
		break;

	default:
			printf("Unknown Viedo Codec\n");
			return;
		
	}

	isAnnexB = false;
	isFirstVideoPacket = true;

	if (isOpen) {
		InternalClose();
	}
	
	InternalOpen (videoFormat, FrameRate);
	SetCurrentPts(avpkt->pts);
	amlResume();
	// GetVideoAxis();
	amlSetVideoAxis(VideoWindowX,VideoWindowY, VideoWindowWidth, VideoWindowHeight);
	//GetVideoAxis();
	
};

int codec_h_ioctl_set(int h, int subcmd, unsigned long  paramter)
{
    int r;
    int cmd_new = AMSTREAM_IOC_SET;
    unsigned long parm_new;

    switch (subcmd) {
    case AMSTREAM_SET_VB_SIZE:
    case AMSTREAM_SET_AB_SIZE:
    case AMSTREAM_SET_VID:
    case AMSTREAM_SET_ACHANNEL:
    case AMSTREAM_SET_SAMPLERATE:
    case AMSTREAM_SET_TSTAMP:
    case AMSTREAM_SET_AID:
    case AMSTREAM_SET_TRICKMODE:
    case AMSTREAM_SET_SID:
    case AMSTREAM_SET_TS_SKIPBYTE:
    case AMSTREAM_SET_PCRSCR:
    case AMSTREAM_SET_SUB_TYPE:
    case AMSTREAM_SET_DEMUX:
    case AMSTREAM_SET_VIDEO_DELAY_LIMIT_MS:
    case AMSTREAM_SET_AUDIO_DELAY_LIMIT_MS:
    case AMSTREAM_SET_DRMMODE: {
        struct am_ioctl_parm parm;
        memset(&parm, 0, sizeof(parm));
        parm.cmd = subcmd;
        parm.data_32 = paramter;
        parm_new = (unsigned long)&parm;
        r = ioctl(h, cmd_new, parm_new);
    }
    break;
    case AMSTREAM_SET_VFORMAT: {
        struct am_ioctl_parm parm;
        memset(&parm, 0, sizeof(parm));
        parm.cmd = subcmd;
        parm.data_vformat = paramter;
        parm_new = (unsigned long)&parm;
        r = ioctl(h, cmd_new,parm_new);
    }
    break;
    case AMSTREAM_SET_AFORMAT: {
        struct am_ioctl_parm parm;
        memset(&parm, 0, sizeof(parm));
        parm.cmd = subcmd;
        parm.data_aformat = paramter;
        parm_new = (unsigned long)&parm;
        r = ioctl(h, cmd_new, parm_new);
    }
    break;
    case AMSTREAM_PORT_INIT:
    case AMSTREAM_AUDIO_RESET:
    case AMSTREAM_SUB_RESET:
    case AMSTREAM_DEC_RESET: {
        struct am_ioctl_parm parm;
        memset(&parm, 0, sizeof(parm));
        parm.cmd = subcmd;
        parm_new = (unsigned long)&parm;
        r = ioctl(h, cmd_new, parm_new);
    }
    break;
    default: {
        struct am_ioctl_parm parm;
        memset(&parm, 0, sizeof(parm));
        parm.cmd = subcmd;
        parm.data_32 = paramter;
        parm_new = (unsigned long)&parm;
        r = ioctl(h, cmd_new, parm_new);
    }
    break;
    }

    if (r < 0) {
        printf("codec_h_ioctl_set failed,handle=%d,cmd=%x,paramter=%x, t=%x errno=%d\n", h, subcmd, paramter, r, errno);
        return r;
    }
    return 0;
}

void InternalOpen(int format, double frameRate)
{
    
#if 0
	this->format = format;
	this->width = width;
	this->height = height;
	this->frameRate = frameRate;
#endif

	// Open codec
	int flags = O_WRONLY;
	switch (format)
	{
		case Hevc:
			//case VideoFormatEnum::VP9:
			handle = open(CODEC_VIDEO_ES_HEVC_DEVICE, flags);
			break;

		default:
			handle = open(CODEC_VIDEO_ES_DEVICE, flags);
			break;
	}

	if (handle < 0)
	{
		
		printf("AmlCodec open failed.\n");
        return;
	}


	// Set video format
	vformat_t amlFormat = (vformat_t)0;
	dec_sysinfo_t am_sysinfo = { 0 };

	//codec.stream_type = STREAM_TYPE_ES_VIDEO;
	//codec.has_video = 1;
	////codec.noblock = 1;

	// Note: Without EXTERNAL_PTS | SYNC_OUTSIDE, the codec auto adjusts
	// frame-rate from PTS 
	//am_sysinfo.param = (void*)(EXTERNAL_PTS | SYNC_OUTSIDE);
	//am_sysinfo.param = (void*)(EXTERNAL_PTS | SYNC_OUTSIDE | USE_IDR_FRAMERATE | UCODE_IP_ONLY_PARAM);
	//am_sysinfo.param = (void*)(SYNC_OUTSIDE | USE_IDR_FRAMERATE);
	//am_sysinfo.param = (void*)(EXTERNAL_PTS | SYNC_OUTSIDE | USE_IDR_FRAMERATE);
	am_sysinfo.param = (void*)(SYNC_OUTSIDE);

	// Rotation (clockwise)
	//am_sysinfo.param = (void*)((unsigned long)(am_sysinfo.param) | 0x10000); //90
	//am_sysinfo.param = (void*)((unsigned long)(am_sysinfo.param) | 0x20000); //180
	//am_sysinfo.param = (void*)((unsigned long)(am_sysinfo.param) | 0x30000); //270


	// Note: Testing has shown that the ALSA clock requires the +1
	am_sysinfo.rate = 96000.0 / frameRate + 0.5;

	//am_sysinfo.width = width;
	//am_sysinfo.height = height;
	//am_sysinfo.ratio64 = (((int64_t)width) << 32) | ((int64_t)height);


	switch (format)
	{
		case Mpeg2:
			Debug(3,"AmlVideoSink - VIDEO/MPEG2\n");
			amlFormat = VFORMAT_MPEG12;
			am_sysinfo.format = VIDEO_DEC_FORMAT_UNKNOW;
			break;

		case Mpeg4V3:
			Debug(3,"AmlVideoSink - VIDEO/MPEG4V3\n");

			amlFormat = VFORMAT_MPEG4;
			am_sysinfo.format = VIDEO_DEC_FORMAT_MPEG4_3;
			break;

		case Mpeg4:
			Debug(3,"AmlVideoSink - VIDEO/MPEG4\n");

			amlFormat = VFORMAT_MPEG4;
			am_sysinfo.format = VIDEO_DEC_FORMAT_MPEG4_5;
			break;

		case Avc:
		{
			// if (width > 1920 || height > 1080)
			// {
			// 	printf("AmlVideoSink - VIDEO/H264_4K2K\n");

			// 	amlFormat = VFORMAT_H264_4K2K;
			// 	am_sysinfo.format = VIDEO_DEC_FORMAT_H264_4K2K;
			// }
			// else
			{
				Debug(3,"AmlVideoSink - VIDEO/H264\n");

				amlFormat = VFORMAT_H264;
				am_sysinfo.format = VIDEO_DEC_FORMAT_H264;
			}
		}
		break;

		case Hevc:
			Debug(3,"AmlVideoSink - VIDEO/HEVC\n");

			amlFormat = VFORMAT_HEVC;
			am_sysinfo.format = VIDEO_DEC_FORMAT_HEVC;
			break;


		case VC1:
			Debug(3,"AmlVideoSink - VIDEO/VC1\n");

			amlFormat = VFORMAT_VC1;
			am_sysinfo.format = VIDEO_DEC_FORMAT_WVC1;
			break;

		default:
			Debug(3,"AmlVideoSink - VIDEO/UNKNOWN(%d)\n", (int)format);
            return;

			
	}


	// S905
	struct am_ioctl_parm parm = { 0 };
	int r;

	if (apiLevel >= S905) // S905
	{
		codec_h_ioctl_set(handle,AMSTREAM_SET_VFORMAT,amlFormat);
	}
	else //S805
	{
		r = ioctl(handle, AMSTREAM_IOC_VFORMAT, amlFormat);
		if (r < 0)
		{
			//codecMutex.Unlock();
			printf("AMSTREAM_IOC_VFORMAT failed.\n");
            return;
		}
	}


	r = ioctl(handle, AMSTREAM_IOC_SYSINFO, (unsigned long)&am_sysinfo);
	if (r < 0)
	{
		//codecMutex.Unlock();
		printf("AMSTREAM_IOC_SYSINFO failed.\n");
        return;
	}


	// Control device
	cntl_handle = open(CODEC_CNTL_DEVICE, O_RDWR);
	if (cntl_handle < 0)
	{
		//codecMutex.Unlock();
		printf("open CODEC_CNTL_DEVICE failed.\n");
        return;
	}

	/*
	if (pcodec->vbuf_size > 0)
	{
	r = codec_h_ioctl(pcodec->handle, AMSTREAM_IOC_SET, AMSTREAM_SET_VB_SIZE, pcodec->vbuf_size);
	if (r < 0)
	{
	return system_error_to_codec_error(r);
	}
	}
	*/

	if (apiLevel >= S905)	//S905
	{
		codec_h_ioctl_set(handle,AMSTREAM_PORT_INIT,0);

	}
	else	//S805
	{
		r = ioctl(handle, AMSTREAM_IOC_PORT_INIT, 0);
		if (r != 0)
		{
			//codecMutex.Unlock();
			printf("AMSTREAM_IOC_PORT_INIT failed.\n");
            return;
		}
	}

	//codec_h_control(pcodec->cntl_handle, AMSTREAM_IOC_SYNCENABLE, (unsigned long)enable);
	r = ioctl(cntl_handle, AMSTREAM_IOC_SYNCENABLE, (unsigned long)1);
	if (r != 0)
	{
		//codecMutex.Unlock();
		printf("AMSTREAM_IOC_SYNCENABLE failed.\n");
        return;
	}

#if 0
	// Restore settings that Kodi tramples
	r = ioctl(cntl_handle, AMSTREAM_IOC_SET_VIDEO_DISABLE, (unsigned long)VIDEO_DISABLE_NONE);
	if (r != 0)
	{
		printf("AMSTREAM_IOC_SET_VIDEO_DISABLE VIDEO_DISABLE_NONE failed.\n");
	}

#endif
	uint32_t screenMode = (uint32_t)VIDEO_WIDEOPTION_NORMAL;
	r = ioctl(cntl_handle, AMSTREAM_IOC_SET_SCREEN_MODE, &screenMode);
	if (r != 0)
	{
		printf("AMSTREAM_IOC_SET_SCREEN_MODE VIDEO_WIDEOPTION_NORMAL failed");
		return;
	}

	isOpen = true;
}

int64_t GetCurrentPts()
{
	//codecMutex.Lock();

	if (!isOpen)
	{
		//codecMutex.Unlock();
		printf("The codec is not open.");
		return 0;
	}


	unsigned int vpts;
	int ret;
	if (apiLevel >= S905)	// S905
	{
		//int vpts = codec_get_vpts(&codec);
		//unsigned int vpts;
		struct am_ioctl_parm parm = { 0 };

		parm.cmd = AMSTREAM_GET_VPTS;
		//parm.data_32 = &vpts;

		ret = ioctl(handle, AMSTREAM_IOC_GET, (unsigned long)&parm);
		if (ret < 0)
		{
			//codecMutex.Unlock();
			printf("AMSTREAM_GET_VPTS failed.");
			return 0;
		}

		vpts = parm.data_32;
		unsigned long vpts = parm.data_64;

		//printf("AmlCodec::GetCurrentPts() parm.data_32=%u parm.data_64=%llu\n",
		//	parm.data_32, parm.data_64);
	}
	else	// S805
	{
		ret = ioctl(handle, AMSTREAM_IOC_VPTS, (unsigned long)&vpts);
		if (ret < 0)
		{
			//codecMutex.Unlock();
			printf("AMSTREAM_IOC_VPTS failed.");
			return 0;
		}
	}

	//codecMutex.Unlock();

	return vpts; // / (double)PTS_FREQ;
}
void SetCurrentPts(double value)
{
	// codecMutex.Lock();

	if (!isOpen)
	{
		//codecMutex.Unlock();
		printf("The codec is not open.\n");
		return;
	}

	//unsigned int pts = value * PTS_FREQ;

	//int codecCall = codec_set_pcrscr(&codec, (int)pts);
	//if (codecCall != 0)
	//{
	//	printf("codec_set_pcrscr failed.\n");
	//}

	// truncate to 32bit
	unsigned long pts = (unsigned long)(value); // * PTS_FREQ);
	pts &= 0xffffffff;

	if (apiLevel >= S905)	// S905
	{
		codec_h_ioctl_set(handle,AMSTREAM_SET_PCRSCR,pts);

	}
	else	// S805
	{
		int ret = ioctl(handle, AMSTREAM_IOC_SET_PCRSCR, pts);
		if (ret < 0)
		{
			//codecMutex.Unlock();
			printf("AMSTREAM_IOC_SET_PCRSCR failed.\n");
			return;
		}
	}

	//codecMutex.Unlock();
}


void ProcessBuffer(AVPacket* pkt)
{
	//playPauseMutex.Lock();

	

	if (doResumeFlag)
	{
		//codec_resume(&codecContext);
		amlResume();

		doResumeFlag = false;
	}


	unsigned char* nalHeader = (unsigned char*)pkt->data;

#if 0
	printf("Header (pkt.size=%x):\n", pkt->size);
	for (int j = 0; j < 25; ++j)	//nalHeaderLength  256
	{
		printf("%02x ", nalHeader[j]);
	}
	printf("\n");
#endif

	if (isFirstVideoPacket)
	{
#if 0
		printf("Header (pkt.size=%x):\n", pkt->size);
		for (int j = 0; j < 16; ++j)	//nalHeaderLength
		{
			printf("%02x ", nalHeader[j]);
		}
		printf("\n");
#endif
		if (nalHeader[0] == 0 && nalHeader[1] == 0 &&
			nalHeader[2] == 1)
		{
			isAnnexB = true;
			isShortStartCode = true;
		}
		else if (nalHeader[0] == 0 && nalHeader[1] == 0 &&
			nalHeader[2] == 0 && nalHeader[3] == 1)
		{
			isAnnexB = true;
			isShortStartCode = false;
		}

		//double timeStamp = av_q2d(buffer->TimeBase()) * pkt->pts;
		//unsigned long pts = (unsigned long)(timeStamp * PTS_FREQ);

		//amlCodec.SetSyncThreshold(pts);

		isFirstVideoPacket = false;

		//printf("isAnnexB=%u\n", isAnnexB);
		//printf("isShortStartCode=%u\n", isShortStartCode);
	}


	uint64_t pts = 0;

	if (pkt->pts != AV_NOPTS_VALUE)
	{
		double timeStamp = av_q2d(timeBase) * pkt->pts;
		pts = (uint64_t)(timeStamp * PTS_FREQ);

		estimatedNextPts = pkt->pts + 3600; // pkt->duration;
		lastTimeStamp = timeStamp;
	}


	isExtraDataSent = false;


	switch (videoFormat)
	{
		case Mpeg2:
		{
			SendCodecData(pts, pkt->data, pkt->size);
			break;
		}

		case Mpeg4:
		{
			//unsigned char* video_extra_data = &extraData[0];
			//int video_extra_data_size = extraData.size();

			//SendCodecData(0, video_extra_data, video_extra_data_size);

			printf("Missing extra Data in mpeg4\n");

			SendCodecData(pts, pkt->data, pkt->size);

			break;
		}
	
		case Avc:
		case Hevc:
		{
			if (!isAnnexB)
			{
				// Five least significant bits of first NAL unit byte signify nal_unit_type.
				int nal_unit_type;
				const int nalHeaderLength = 4;

				while (nalHeader < (pkt->data + pkt->size))
				{
					switch (videoFormat)
					{
#if 0
						case Avc:
						{
							// Copy AnnexB data if NAL unit type is 5
							nal_unit_type = nalHeader[nalHeaderLength] & 0x1F;

							if (!isExtraDataSent || nal_unit_type == 5)
							{
								ConvertH264ExtraDataToAnnexB();

								SendCodecData(pts, &videoExtraData[0], videoExtraData.size());
								//amlCodec.SendData(pts, &videoExtraData[0], videoExtraData.size());
							}

							isExtraDataSent = true;
						}
						break;
#endif
						case Hevc:
						{
							nal_unit_type = (nalHeader[nalHeaderLength] >> 1) & 0x3f;

							/* prepend extradata to IRAP frames */
							if (!isExtraDataSent || (nal_unit_type >= 16 && nal_unit_type <= 23))
							{
								//HevcExtraDataToAnnexB();
								printf("------------------Extra Data not sent !!!!!!!!!!!!!");

								//SendCodecData(0, &videoExtraData[0], videoExtraData.size());
								//amlCodec.SendData(0, &videoExtraData[0], videoExtraData.size());
							}

							isExtraDataSent = true;
						}
						break;

						default:
							printf("Unexpected video format.\n");
							return;
					}


					// Overwrite header NAL length with startcode '0x00000001' in *BigEndian*
					int nalLength = nalHeader[0] << 24;
					nalLength |= nalHeader[1] << 16;
					nalLength |= nalHeader[2] << 8;
					nalLength |= nalHeader[3];

					if (nalLength < 0 || nalLength > pkt->size)
					{
						printf("Invalid NAL length=%d, pkt->size=%d\n", nalLength, pkt->size);
						return;
					}

					nalHeader[0] = 0;
					nalHeader[1] = 0;
					nalHeader[2] = 0;
					nalHeader[3] = 1;

					nalHeader += nalLength + 4;
				}
			}

			if (!SendCodecData(pts, pkt->data, pkt->size))
			{
				// Resend extra data on codec reset
				isExtraDataSent = false;

				printf("AmlVideoSinkElement::ProcessBuffer - SendData Failed.\n");
			}

			break;
		}

		case VC1:
		{
			SendCodecData(pts, pkt->data, pkt->size);

			break;
		}

		default:
			printf("Codec not Supported\n");
			return;
	}


	if (doPauseFlag)
	{
		//codec_pause(&codecContext);
		//amlCodec.Pause();
		doPauseFlag = false;
	}

	//playPauseMutex.Unlock();
}

CheckinPts(unsigned long pts)
{
	//codecMutex.Lock();


	if (!isOpen)
	{
		//codecMutex.Unlock();
		printf("The codec is not open. %s\n",__FUNCTION__);
		return;
	}


	// truncate to 32bit
	//pts &= 0xffffffff;

	if (apiLevel >= S905)	// S905
	{
		codec_h_ioctl_set(handle,AMSTREAM_SET_TSTAMP,pts);
	}
	else	// S805
	{
		int r = ioctl(handle, AMSTREAM_IOC_TSTAMP, pts);
		if (r < 0)
		{
			//codecMutex.Unlock();
			printf("AMSTREAM_IOC_TSTAMP failed\n");
			return;
		}
	}

	//codecMutex.Unlock();
}

int WriteData(unsigned char* data, int length)
{
	if (data == NULL) {
		printf("data");
		return;
	}

	if (length < 1) {
		printf("length");
		return;
	}


	// This is done unlocked because it blocks
	// int written = 0;
	// while (written < length)
	// {
		int ret = write(handle, data, length);
		if (ret == length) {
			usleep(2000);
		}
	// 	if (ret < 0)
	// 	{
	// 		if (errno == EAGAIN)
	// 		{
	// 			//printf("EAGAIN.\n");
	// 			sleep(0);

	// 			ret = 0;
	// 		}
	// 		else
	// 		{
	// 			printf("write failed. (%d)(%d)\n", ret, errno);
	// 			abort();
	// 		}
	// 	}

	// 	written += ret;
	// }

	return ret; //written;
}

Bool SendCodecData(unsigned long pts, unsigned char* data, int length)
{
	//printf("AmlVideoSink: SendCodecData - pts=%lu, data=%p, length=0x%x\n", pts, data, length);
	Bool result = true;

	if (pts > 0)
	{
		CheckinPts(pts);
	}

	int maxAttempts = 150;
	int offset = 0;
	while (offset < length)
	{
		if (!isRunning)
		{
			result = false;
			break;
		}

		int count = WriteData(data + offset, length - offset);
		if (count > 0)
		{
			offset += count;
			//printf("codec_write send %x bytes of %x total.\n", count, length);
		}
		else
		{
			//printf("codec_write failed for (%x).\n",length - offset, count);
			//amlGetBufferStatus();
			maxAttempts -= 1;

			if (maxAttempts <= 0)
			{
				//printf("codec_write max attempts exceeded.\n");
				
				amlReset();
				result = false;

				break;
			}

			sleep(0);
		}
	}

	return result;
}

amlPause()
{
	//codecMutex.Lock();

	if (!isOpen)
	{
		//codecMutex.Unlock();
		printf("The codec is not open. %s\n",__FUNCTION__);
		return;
	}

	
	//codec_pause(&codec);
	int ret = ioctl(cntl_handle, AMSTREAM_IOC_VPAUSE, 1);
	if (ret < 0)
	{
		//codecMutex.Unlock();
		printf("AMSTREAM_IOC_VPAUSE (1) failed.");
	}

	//codecMutex.Unlock();
}
amlResume()
{
	//codecMutex.Lock();

	if (!isOpen)
	{
		//codecMutex.Unlock();
		//printf("The codec is not open. %s\n",__FUNCTION__);
		return;
	}

	
	//codec_resume(&codec);
	int ret = ioctl(cntl_handle, AMSTREAM_IOC_VPAUSE, 0);
	if (ret < 0)
	{
		//codecMutex.Unlock();
		printf("AMSTREAM_IOC_VPAUSE (0) failed.\n");
	}
    isRunning = true;
	//codecMutex.Unlock();
}

amlClearVideo()
{
	//codecMutex.Lock();

	if (!isOpen)
	{
		//codecMutex.Unlock();
		printf("The codec is not open. %s\n",__FUNCTION__);
		return;
	}

	
	//codec_resume(&codec);
	int ret = ioctl(cntl_handle, AMSTREAM_IOC_CLEAR_VIDEO, 0);
	if (ret < 0)
	{
		//codecMutex.Unlock();
		printf("AMSTREAM_IOC_CLEAR_VIDEO (0) failed.\n");
	}
    printf("clear video\n");
	//codecMutex.Unlock();
}

amlReset()
{
	// codecMutex.Lock();

	if (!isOpen)
	{
		//codecMutex.Unlock();
		printf("The codec is not open. %s\n",__FUNCTION__);
	}
	// set the system blackout_policy to leave the last frame showing
	int blackout_policy;
	amlGetInt("/sys/class/video/blackout_policy", &blackout_policy);
	amlSetInt("/sys/class/video/blackout_policy", 0);

	//amlPause();
;
	InternalClose();
	InternalOpen(videoFormat,FrameRate);

	//amlSetVideoDelayLimit(1000);

	amlSetInt("/sys/class/video/blackout_policy", blackout_policy);
	
	//printf("amlReset\n");
	
	//codecMutex.Unlock();
}

void InternalClose()
{
	int r;

	//amlClearVideo();

	r = close(cntl_handle);
	if (r < 0)
	{
		//codecMutex.Unlock();
		printf("close cntl_handle failed.");
		return;
	}


	r = close(handle);
	if (r < 0)
	{
		//codecMutex.Unlock();
		printf("close handle failed.");
		return;
	}

	cntl_handle = -1;
	handle = -1;

	isOpen = false;
}

void amlSetVideoAxis(int x, int y, int width, int height)
{
	//codecMutex.Lock();

	if (!isOpen)
	{
		//codecMutex.Unlock();
		printf("The codec is not open. %s\n",__FUNCTION__);
		return;
	}

	int params[4] = { x, y, width, height };
		

	int ret = ioctl(cntl_handle, AMSTREAM_IOC_SET_VIDEO_AXIS, &params);

	//codecMutex.Unlock();

	if (ret < 0)
	{
		printf("AMSTREAM_IOC_SET_VIDEO_AXIS failed.");
	}
}

void amlGetVideoAxis()
{
	//codecMutex.Lock();

	if (!isOpen)
	{
		//codecMutex.Unlock();
		printf("The codec is not open. %s\n",__FUNCTION__);
		return;
	}

	int params[4] = { 0 };

	int ret = ioctl(cntl_handle, AMSTREAM_IOC_GET_VIDEO_AXIS, &params);

	//codecMutex.Unlock();

	if (ret < 0)
	{
		printf("AMSTREAM_IOC_GET_VIDEO_AXIS failed.");
	}

	//printf("Video Axis %d %d - %d %d\n",params[0],params[1],params[2],params[3]);
		
	return;
}


int amlGetBufferStatus()
{
	//codecMutex.Lock();

	if (!isOpen)
	{
		//codecMutex.Unlock();
		printf("The codec is not open. %s\n",__FUNCTION__);
		return;
	}

	
	struct buf_status status;
	if (apiLevel >= S905)	// S905
	{
		struct am_ioctl_parm_ex parm = { 0 };
		parm.cmd = AMSTREAM_GET_EX_VB_STATUS;
		
		int r = ioctl(handle, AMSTREAM_IOC_GET_EX, (unsigned long)&parm);

		//codecMutex.Unlock();

		if (r < 0)
		{
			printf("AMSTREAM_GET_EX_VB_STATUS failed.");
			return 0;
		}

		memcpy(&status, &parm.status, sizeof(status));
	}
	else	// S805
	{
		struct am_io_param am_io;

		int r = ioctl(handle, AMSTREAM_IOC_VB_STATUS, (unsigned long)&am_io);

		//codecMutex.Unlock();

		if (r < 0)
		{
			printf("AMSTREAM_IOC_VB_STATUS failed.");
			return 0;
		}

		memcpy(&status, &am_io.status, sizeof(status));
	}
	//rintf("STatus: write %u read %u free %d size %d data %d\n",status.write_pointer,status.read_pointer,status.free_len,status.size,status.data_len);
	return status.data_len;
}

void amlClearVBuf()
{
	//codecMutex.Lock();

	if (!isOpen)
	{
		//codecMutex.Unlock();
		printf("The codec is not open. %s\n",__FUNCTION__);
		return;
	}

	
	struct buf_status status;
	if (apiLevel >= S905)	// S905
	{
		struct am_ioctl_parm_ex parm = { 0 };
		parm.cmd = AMSTREAM_CLEAR_VBUF;
		
		int r = ioctl(handle, AMSTREAM_IOC_SET_EX, (unsigned long)&parm);

		//codecMutex.Unlock();

		if (r < 0)
		{
			printf("AMSTREAM_CLEARVBUF failed. %d \n",r);
			return;
		}

		
	}
}

void amlSetVideoDelayLimit(int ms)
{
	
    codec_h_ioctl_set (handle, AMSTREAM_SET_VIDEO_DELAY_LIMIT_MS, ms);

}

void amlDecReset()
{
	//codecMutex.Lock();

	if (!isOpen)
	{
		//codecMutex.Unlock();
		printf("The codec is not open. %s\n",__FUNCTION__);
		return;
	}
	
	//int r = ioctl(cntl_handle, AMSTREAM_IOC_CLEAR_VBUF);
	int r = ioctl(cntl_handle, AMSTREAM_IOC_SET_DEC_RESET,(unsigned long)1);	
	//codecMutex.Unlock();

	if (r < 0)
	{
		printf("AMSTREAM_DEC_RESET failed.");
		return;
	}	
}

void amlFreerun(int val)
{
	//codecMutex.Lock();

	if (!isOpen)
	{
		//codecMutex.Unlock();
		//printf("The codec is not open. %s\n",__FUNCTION__);
		return;
	}
	
	int r = ioctl(cntl_handle, AMSTREAM_IOC_SET_FREERUN_MODE,(unsigned long)val);
	//int r = ioctl(cntl_handle, AMSTREAM_IOC_TRICKMODE ,val);
	//codecMutex.Unlock();

	if (r < 0)
	{
		printf("AMSTREAM_FREERUN failed. %d",val);
		return;
	}	
}

void amlTrickMode(int val)
{
	//codecMutex.Lock();

	if (!isOpen)
	{
		//codecMutex.Unlock();
		printf("The codec is not open. %s\n",__FUNCTION__);
		return;
	}
	
	
	int r = ioctl(cntl_handle, AMSTREAM_IOC_TRICKMODE ,(unsigned long)val);
	//codecMutex.Unlock();

	if (r < 0)
	{
		printf("AMSTREAM_TRICKMODE failed. %d",val);
		return;
	}	
}

int amlSetString(char *path, char *valstr)
{
  int fd = open(path, O_RDWR, 0644);
  int ret = 0;
  if (fd >= 0)
  {
    if (write(fd, valstr, sizeof(*valstr)) < 0)
      ret = -1;
    close(fd);
  }
  if (ret)
    Debug(3, "%s: error writing %s",__FUNCTION__, path);

  return ret;
}

int amlGetString(char *path, char *valstr)
{
  int len;
  char buf[256] = {0};

  int fd = open(path, O_RDONLY);
  if (fd >= 0)
  {
    *valstr = 0;
    while ((len = read(fd, buf, 256)) > 0)
      strncat(valstr,buf,len);
    close(fd);

    //StringUtils::Trim(valstr);

    return 0;
  }

  Debug(3, "%s: error reading %s",__FUNCTION__, path);
  valstr = "fail";
  return -1;
}

int amlSetInt(char *path, int val)
{
  int fd = open(path, O_RDWR, 0644);
  int ret = 0;
  if (fd >= 0)
  {
    char bcmd[16];
    sprintf(bcmd, "%d", val);
    if (write(fd, bcmd, strlen(bcmd)) < 0)
      ret = -1;
    close(fd);
  }
  if (ret)
    Debug(3, "%s: error writing %s",__FUNCTION__, path);

  return ret;
}

int amlGetInt(char *path, int *val)
{
  int fd = open(path, O_RDONLY);
  int ret = 0;
  long res;
  if (fd >= 0)
  {
    char bcmd[16];
    if (read(fd, bcmd, sizeof(bcmd)) < 0)
      ret = -1;
    else
      res = strtol(bcmd, NULL, 16);

    close(fd);
  }
  if (ret)
    Debug(3, "%s: error reading %s",__FUNCTION__, path);
  *val = res;
  return ret;
}

bool amlHas(char *path)
{
  int fd = open(path, O_RDONLY);
  if (fd >= 0)
  {
    close(fd);
    return true;
  }
  return false;
}

bool amlHasRW(char *path)
{
  int fd = open(path, O_RDWR);
  if (fd >= 0)
  {
    close(fd);
    return true;
  }
  return false;
}
