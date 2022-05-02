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
//#include <X11/Xlib.h>

#include "codec_type.h"
#include "amports/amstream.h"

#define VIDEO_DISABLE_NONE    (0)
enum {
	VIDEO_WIDEOPTION_NORMAL = 0,
	VIDEO_WIDEOPTION_FULL_STRETCH = 1,
	VIDEO_WIDEOPTION_4_3 = 2,
	VIDEO_WIDEOPTION_16_9 = 3,
	VIDEO_WIDEOPTION_NONLINEAR = 4,
	VIDEO_WIDEOPTION_NORMAL_NOSCALEUP = 5,
	VIDEO_WIDEOPTION_4_3_IGNORE = 6,
	VIDEO_WIDEOPTION_4_3_LETTER_BOX = 7,
	VIDEO_WIDEOPTION_4_3_PAN_SCAN = 8,
	VIDEO_WIDEOPTION_4_3_COMBINED = 9,
	VIDEO_WIDEOPTION_16_9_IGNORE = 10,
	VIDEO_WIDEOPTION_16_9_LETTER_BOX = 11,
	VIDEO_WIDEOPTION_16_9_PAN_SCAN = 12,
	VIDEO_WIDEOPTION_16_9_COMBINED = 13,
	VIDEO_WIDEOPTION_CUSTOM = 14,
	VIDEO_WIDEOPTION_AFD = 15,
	VIDEO_WIDEOPTION_MAX = 16
};

#include "video.h"
#include "codec.h"
#include "audio.h"
#include "misc.h"

#define False 0
#define True 1
typedef int Bool;

#define false False
#define true True
#define bool Bool

typedef struct
{ 
    uint32_t x0; 
    uint32_t y0;
    uint32_t x1;
    uint32_t y1;
} VdpRect;

OdroidDecoder *OdroidDecoders[2];  ///< open decoder streams
static int OdroidDecoderN = 0;				 // Numer of decoders
static struct timespec OdroidFrameTime;  ///< time of last display
static int VideoWindowX = 0;                ///< video output window x coordinate
static int VideoWindowY = 0;                ///< video outout window y coordinate
int VideoWindowWidth = 1920;       ///< video output window width
int VideoWindowHeight = 1080;      ///< video output window height
static int OsdConfigWidth;              ///< osd configured width
static int OsdConfigHeight;             ///< osd configured height
static int OsdWidth;                    ///< osd width
static int OsdHeight;                   ///< osd height
int OsdShown = 0;
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
int isPIP = false;
int PIP_allowed = false;
double lastTimeStamp = -1;
bool isFirstVideoPacket = true;
bool isAnnexB = false;
bool isShortStartCode = false;
bool isExtraDataSent = false;
uint64_t FirstVPTS;
int64_t estimatedNextPts = 0;
int Hdr2Sdr = 0;
int NoiseReduction = 1;
int use_pip=0,use_pip_mpeg2=0;

const uint64_t PTS_FREQ = 90000;
int64_t LastPTS;

bool doPauseFlag = false;
bool doResumeFlag = false;


AVRational timeBase;

int handle,cntl_handle,fd,DmaBufferHandle;


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

	amlSetVideoAxis(decoder->pip,x,y,x+width,y+height);
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

 int64_t VideoGetClock(const VideoHwDecoder *i) {
	 
	 return LastPTS;

 };


/// Draw an OSD ARGB image.
 void VideoOsdDrawARGB(int i, int j, int k, int l, int m, const uint8_t *n, int o, int p) {};

/// Get OSD size.
void VideoGetOsdSize(int *width, int *height) {

    *width = 0;
    *height = 0;

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

/// Grab screen raw.
 uint8_t *VideoGrabService(int *size, int *width, int *height) {
	 return VideoGrab(size,width,height,0);
 };


#define ION_IOC_MESON_PHYS_ADDR             8

struct meson_phys_data {
	int handle;
	unsigned int phys_addr;
	unsigned int size;
};

#define CAP_FLAG_AT_CURRENT		0
#define CAP_FLAG_AT_TIME_WINDOW	1
#define CAP_FLAG_AT_END			2
#define AMVIDEOCAP_IOC_MAGIC 'V'
#define AMVIDEOCAP_IOW_SET_WANTFRAME_FORMAT _IOW(AMVIDEOCAP_IOC_MAGIC, 0x01, int)
#define AMVIDEOCAP_IOW_SET_WANTFRAME_WIDTH  _IOW(AMVIDEOCAP_IOC_MAGIC, 0x02, int)
#define AMVIDEOCAP_IOW_SET_WANTFRAME_HEIGHT _IOW(AMVIDEOCAP_IOC_MAGIC, 0x03, int)
#define AMVIDEOCAP_IOW_SET_WANTFRAME_WAIT_MAX_MS _IOW(AMVIDEOCAP_IOC_MAGIC, 0x05, uint64_t)
#define AMVIDEOCAP_IOW_SET_WANTFRAME_AT_FLAGS _IOR(AMVIDEOCAP_IOC_MAGIC, 0x06, int)
#define AMVIDEOCAP_IOW_GET_STATE            _IOR(AMVIDEOCAP_IOC_MAGIC, 0x31, int)
#define AMVIDEOCAP_IOW_SET_START_CAPTURE    _IOW(AMVIDEOCAP_IOC_MAGIC, 0x32, int)

int GrabOsd(char *base, int width, int height) {

	struct fb_var_screeninfo vinfo;
	unsigned capSize, bytesPerPixel;
	int pixelFormat;

	int _fbfd = open("/dev/fb0", O_RDONLY);
	if (_fbfd < 0) {
		printf("Unable to open fd0\n");
		return false;
	}

	/* get variable screen information */
	ioctl (_fbfd, FBIOGET_VSCREENINFO, &vinfo);

	bytesPerPixel = vinfo.bits_per_pixel / 8;

	capSize = VideoWindowWidth * VideoWindowHeight * bytesPerPixel;
		
	/* map the device to memory */
	char *_fbp = (unsigned char*)mmap(0, capSize, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, _fbfd, 0);
	if (!_fbp)	{
		printf("Unable to MMAP fb0\n");
		return false;
	}
	memcpy(base,_fbp,capSize);
	munmap(_fbp, capSize);
	close(_fbfd);
	return true;
}

uint8_t GrabVideo(char *base, int width, int height) {

	
// If the device is not open, attempt to open it
	//printf("capture Video %d %d\n",width,height);
	
	int _amlogicCaptureDev = open("/dev/amvideocap0", O_RDONLY, 0);

	// If the device is still not open, there is something wrong
	if (_amlogicCaptureDev == -1)
	{
		Debug(3,"No Capture device found");
		return false;
	}

	int stride = ALIGN(width, 16) * 4;
	int format = GE2D_FORMAT_S32_ARGB;
	
	if (ioctl(_amlogicCaptureDev, AMVIDEOCAP_IOW_SET_WANTFRAME_WIDTH,  stride / 4)  == -1 ||
		ioctl(_amlogicCaptureDev, AMVIDEOCAP_IOW_SET_WANTFRAME_HEIGHT, height) == -1 ||
		ioctl(_amlogicCaptureDev, AMVIDEOCAP_IOW_SET_WANTFRAME_FORMAT, format) == -1 )
	{
		// Failed to configure frame width
		Debug(3,"Failed to configure capture size %d %d\n",width,height);
		close(_amlogicCaptureDev);
		return false;
	}
	int state;
	//ioctl(_amlogicCaptureDev, AMVIDEOCAP_IOW_GET_STATE, &state);
	//printf("Got Cap State %d\n",state);
	uint64_t waitms = 5000;
	//ioctl(_amlogicCaptureDev, AMVIDEOCAP_IOW_SET_WANTFRAME_WAIT_MAX_MS, waitms);
	//ioctl(_amlogicCaptureDev, AMVIDEOCAP_IOW_SET_WANTFRAME_AT_FLAGS, CAP_FLAG_AT_TIME_WINDOW);
	//WaitVsync();
	//ioctl(_amlogicCaptureDev, AMVIDEOCAP_IOW_SET_START_CAPTURE, 1);
	
	// Read the snapshot into the memory
	//ioctl(_amlogicCaptureDev, AMVIDEOCAP_IOW_GET_STATE, &state);
	//printf("Got Cap State %d\n",state);
	
	const ssize_t bytesToRead = stride * height;

	const ssize_t bytesRead   = pread(_amlogicCaptureDev, base, bytesToRead, 0);
	close(_amlogicCaptureDev);

	if (bytesRead == -1)
	{
		Debug(3,"Read of capture device failed");
		return false;
	}
	else if (bytesToRead != bytesRead)
	{
		// Read of snapshot failed
		Debug(3,"Capture failed to grab entire image [bytesToRead(%d) != bytesRead(%d)]",bytesToRead,bytesRead);
		return false;
	}

	if (stride / 4 != width) {     // we need to shift it together
		char *dst = base + width;
		char *src = base + stride  ;
		for (int i=1; i < height ;i++) {
			memcpy(dst,src,width);
			dst += width;
			src += stride / 4;
		}

	}
	// For now we always close the device now and again

	return true;
}

static int scan_str(const char* buf, const char* pattern)
{
       int res = 0;
       if (buf && pattern) {
               const char* ptr = strstr(buf, pattern);
               if (ptr != NULL) {
                       char format[strlen(pattern)+5];
                       strcpy(format,pattern);
                       strcat(format,"%d");
                       sscanf(ptr, format, &res);
               }
       }
       return res;
}

                      
///
/// Grab output surface already locked.
///
/// @param ret_size[out]    size of allocated surface copy
/// @param ret_width[in,out]    width of output
/// @param ret_height[in,out]   height of output
///
uint8_t *OdroidVideoGrab(int *ret_size, int *ret_width, int *ret_height, int mitosd)
{
    uint32_t size;
    uint32_t width;
    uint32_t height;
    uint8_t *base, c;
    VdpRect source_rect;

    if (!isOpen)                // no video aktiv
        return NULL;

	char vdec_status[512];
	amlGetString("/sys/class/vdec/vdec_status",vdec_status,sizeof(vdec_status));
	
	if(strstr(vdec_status,"No vdec")) {
		return NULL;
	}
	
    width = scan_str(vdec_status,"width : ");
	height = scan_str(vdec_status,"height : ");
	
	//printf("Video is %d-%d\n",width,height);
    // get real surface size

//	if ( (mitosd & OsdShown)) {
		width = OsdWidth;
		height = OsdHeight;
//	}

    // Debug(3, "video/cuvid: grab %dx%d\n", width, height);

    source_rect.x0 = 0;
    source_rect.y0 = 0;
    source_rect.x1 = width;
    source_rect.y1 = height;

    if (ret_width && ret_height) {
        if (*ret_width <= -64) {        // this is an Atmo grab service request
            int overscan;
            // calculate aspect correct size of analyze image
            width = *ret_width * -1;
            height = (width * source_rect.y1) / source_rect.x1;

            // calculate size of grab (sub) window
            overscan = *ret_height;

            if (overscan > 0 && overscan <= 200) {
                source_rect.x0 = source_rect.x1 * overscan / 1000;
                source_rect.x1 -= source_rect.x0;
                source_rect.y0 = source_rect.y1 * overscan / 1000;
                source_rect.y1 -= source_rect.y0;
            }
        }
#if 0
		else {
            if (*ret_width > 0 && (unsigned)*ret_width < width) {
                width = *ret_width;
            }
            if (*ret_height > 0 && (unsigned)*ret_height < height) {
                height = *ret_height;
            }
        }
#endif
        //printf("video/cuvid: grab source  dim %dx%d\n", width, height);

        size = width * height * sizeof(uint32_t);

        base = malloc(size);

        if (!base) {
            Debug(3,"video/cuvid: out of memory\n");
            return NULL;
        }
        
		if (!GrabVideo(base,width,height)) {
			free(base);
			return NULL;
		}

		if (mitosd && OsdShown) {
			char *osd = malloc(VideoWindowWidth * VideoWindowHeight * 4);
			if (!GrabOsd(osd,OsdWidth,OsdHeight)) {
				free(osd);
				return NULL;
			}
			int stride = VideoWindowWidth * 4;
			char *sb = base;
			for (int y = 0; y < OsdHeight; ++y)
			{
				unsigned char *videoPtr = osd + y * stride;
				
				for (int x = 0; x < OsdWidth; ++x, base += 4, videoPtr += 4)
				{
					float alpha = videoPtr[3] / (float)255;

					//B
					base[0] = (1 -alpha) * (float)base[0] + alpha * (float)videoPtr[0];
					//G
					base[1] = (1 -alpha) * (float)base[1] + alpha * (float)videoPtr[1];
					//R
					base[2] = (1- alpha) * (float)base[2] + alpha * (float)videoPtr[2];
					//A
					base[3] = 0xff;// we are solid now
					
				}	
			}
			free(osd);
			base = sb;
		}

        if (ret_size) {
            *ret_size = size;
        }
        if (ret_width) {
            *ret_width = width;
        }
        if (ret_height) {
            *ret_height = height;
        }
        return base;
    }

    return NULL;
}

uint8_t *VideoGrab(int *size, int *width, int *height, int write_header)
{
   	uint8_t *data;
	uint8_t *rgb;
	char buf[64];
	int i;
	int n;
	int scale_width;
	int scale_height;
	int x;
	int y;
	double src_x;
	double src_y;
	double scale_x;
	double scale_y;

	scale_width = *width;
	scale_height = *height;
	n = 0;
	
	data = OdroidVideoGrab(size, width, height, 1);
	
	if (data == NULL)
		return NULL;

	if (scale_width <= 0) {
		scale_width = *width;
	}
	if (scale_height <= 0) {
		scale_height = *height;
	}
	// hardware didn't scale for us, use simple software scaler
	if (scale_width != *width && scale_height != *height) {

		if (write_header) {
			n = snprintf(buf, sizeof(buf), "P6\n%d\n%d\n255\n", scale_width, scale_height);
		}
		rgb = malloc(scale_width * scale_height * 3 + n);
		if (!rgb) {
			Debug(3,"video: out of memory\n");
			free(data);
			return NULL;
		}
		*size = scale_width * scale_height * 3 + n;
		memcpy(rgb, buf, n);        // header

		scale_x = (double)*width / scale_width;
		scale_y = (double)*height / scale_height;

		src_y = 0.0;
		for (y = 0; y < scale_height; y++) {
			int o;

			src_x = 0.0;
			o = (int)src_y **width;

			for (x = 0; x < scale_width; x++) {
				i = 4 * (o + (int)src_x);

				rgb[n + (x + y * scale_width) * 3 + 0] = data[i + 2];
				rgb[n + (x + y * scale_width) * 3 + 1] = data[i + 1];
				rgb[n + (x + y * scale_width) * 3 + 2] = data[i + 0];

				src_x += scale_x;
			}

			src_y += scale_y;
		}

		*width = scale_width;
		*height = scale_height;

		// grabed image of correct size convert BGRA -> RGB
	} else {
		if (write_header) {
			n = snprintf(buf, sizeof(buf), "P6\n%d\n%d\n255\n", *width, *height);
		}
		rgb = malloc(*width * *height * 3 + n);
		if (!rgb) {
			Debug(3,"video: out of memory\n");
			free(data);
			return NULL;
		}
		memcpy(rgb, buf, n);        // header

		for (i = 0; i < *size / 4; ++i) {   // convert bgra -> rgb
			rgb[n + i * 3 + 0] = data[i * 4 + 2];
			rgb[n + i * 3 + 1] = data[i * 4 + 1];
			rgb[n + i * 3 + 2] = data[i * 4 + 0];
		}

		*size = *width * *height * 3 + n;
	}
	free(data); 

	return rgb;
}

/// Get decoder statistics.
 void VideoGetStats(VideoHwDecoder *i, int *j, int *k, int *l, int *m, float *n, int *o, int *p, int *q, int *r) {};

/// Get video stream size
void VideoGetVideoSize(VideoHwDecoder *i, int *width, int *height, int *aspect_num, int *aspect_den) {
	char vdec_status[512];

	if (amlGetString("/sys/class/vdec/vdec_status",vdec_status,sizeof(vdec_status)) < 0 ||
          !*vdec_status || strstr(vdec_status,"No vdec")) {
		*width = *height = 0;
		*aspect_num = *aspect_den = 1;
		return;
	}
	
	*width  = scan_str(vdec_status,"width : ");
    *height = scan_str(vdec_status,"height : ");
    int r   = scan_str(vdec_status,"ratio_control : ");
    if (r == 9000) {
		*aspect_num = 16;
		*aspect_den = 9;
	}
	else {
		*aspect_num = 4;
		*aspect_den = 3;
	}
};

 void VideoOsdInit(void) {

	//OsdWidth = VideoWindowWidth;
	//OsdHeight = VideoWindowHeight;
 	
 };         ///< Setup osd


 void VideoOsdExit(void) {};         ///< Cleanup osd.


 void VideoExit(void) {
	
	// Restore alpha setting
	int fd_m;
	struct fb_var_screeninfo info;

	Debug(3,"VideoExit");

	//InternalClose(OdroidDecoders[0]->pip);

	VideoThreadExit();
	sleep(1);

	for (int i = 0; i < OdroidDecoderN; ++i) {
        if (OdroidDecoders[i]) {
            VideoDelHwDecoder(OdroidDecoders[i]);
            OdroidDecoders[i] = NULL;
        }
    }
    OdroidDecoderN = 0;

	int r = close(cntl_handle);
	if (r < 0)
	{
		//codecMutex.Unlock();
		printf("close cntl_handle failed.");
		//return;
	}

// Set text mode
#if 0
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
			//return;
		}
		close(ttyfd);
	}
#endif
#if 1
	fd_m = open("/dev/fb0", O_RDWR);
	ioctl(fd_m, FBIOGET_VSCREENINFO, &info);
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
	info.bits_per_pixel = 32;
	info.nonstd = 1;
	info.xres = 1920;
	info.yres = 1079;
	info.xres_virtual = 1920;
	info.yres_virtual = (1080*2);
	ioctl(fd_m, FBIOPUT_VSCREENINFO, &info);
	close(fd_m);
#endif
	//close (ge2d_fd);

	amlSetInt("/sys/class/graphics/fb0/free_scale", 0);

	// restore vfm mapping
	amlSetString("/sys/class/vfm/map","rm all");
	amlSetString("/sys/class/vfm/map","add default decoder amvideo");
	amlSetString("/sys/class/vfm/map","add default_amlvideo2 vdin1 amlvideo2.1");
	amlSetString("/sys/class/vfm/map","add dvblpath dvbldec amvideo");
	amlSetString("/sys/class/vfm/map","add dvelpath dveldec dvel");
	amlSetString("/sys/class/vfm/map","add dvhdmiin dv_vdin amvideo");

	// reset audio codec to 2 chan
	amlSetInt("/sys/class/audiodsp/digital_codec", 0);

//	amlSetInt("/sys/class/graphics/fb1/free_scale", 0);

	
 };            ///< Cleanup and exit video module.


/// Set DPMS at Blackscreen switch
 void SetDPMSatBlackScreen(int i) {};

/// Raise the frontend window
 int VideoRaiseWindow(void) {};

/// Set Shaders
 int VideoSetShader(char *i) {};


/// Deallocate a video decoder context.
void CodecVideoDelDecoder(VideoDecoder *decoder){
	free(decoder);
};

/// Close video codec.
 void CodecVideoClose(VideoHwDecoder *HwDecoder) {
	Debug(3,"CodecVideoClose Pip %d Handle %d\n",HwDecoder->pip,HwDecoder->handle);
	InternalClose(HwDecoder->pip);
 };

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

void VideoDelHwDecoder(VideoHwDecoder * decoder)
{
	Debug(3,"DelHWDecoder PIP %d ",decoder->pip);
	if (decoder->pip && OdroidDecoders[1]->handle != -1) {
		isPIP = false;
		InternalClose(1);
	}

	for (int i = 0; i < OdroidDecoderN; ++i) {
        if (OdroidDecoders[i] == decoder) {
            OdroidDecoders[i] = NULL;
			
            // copy last slot into empty slot
		
            if (i < --OdroidDecoderN) {
                OdroidDecoders[i] = OdroidDecoders[OdroidDecoderN];
            }
			          
            free(decoder);
            return;
        }
    }

}

extern uint64_t AudioGetClock(void);
extern uint64_t GetCurrentVPts(int);

extern int SetCurrentPCR(int, uint64_t );
extern int AHandle;
void ProcessClockBuffer(int handle)
	{

		// truncate to 32bit
		uint64_t apts;
		uint64_t pts = apts = (uint64_t)AudioGetClock(); //(uint64_t) GetCurrentAPts(AHandle) ; 
		pts &= 0xffffffff;

		uint64_t vpts = (uint64_t)GetCurrentVPts(handle) ;	
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
				
				SetCurrentPCR(handle,apts);

				//printf("AmlVideoSink: Adjust PTS - apts= %#012" PRIx64 " vpts %#012" PRIx64 "   (%f frames)\n", pts , vpts , driftFrames);
			}
		}

	}

void CodecVideoDecode(VideoDecoder * decoder, const AVPacket * avpkt) 
{	
	int pip = decoder->HwDecoder->pip;
	int handle = decoder->HwDecoder->handle;
	if (pip) {
		ProcessBuffer(decoder->HwDecoder,avpkt);
		return;
	}
	decoder->PTS = avpkt->pts;   		// save last pts;
	if (!decoder->HwDecoder->TrickSpeed)
		ProcessClockBuffer(handle);
	else {
		if (avpkt->pts != AV_NOPTS_VALUE) {
			SetCurrentPCR(handle,avpkt->pts);
			//printf("push buffer ohne sync PTS %ld\n",avpkt->pts);
		}
	}
	ProcessBuffer(decoder->HwDecoder,avpkt);
	if (decoder->HwDecoder->TrickSpeed) {
		usleep(20000*decoder->HwDecoder->TrickSpeed);
	}
	//amlGetBufferStatus();
};


void ClearDisplay(void)
{
	int io;
   extern int ge2d_fd;
	amlSetInt("/sys/class/graphics/fb0/osd_clear", 1);

	OsdShown = 0;
    return;
	
#if 0
	// Configure src/dst
    struct config_para_ex_ion_s fill_config = { 0 };
    fill_config.alu_const_color = 0xffffffff;

    fill_config.src_para.mem_type = CANVAS_OSD0;
    fill_config.src_para.format = GE2D_FORMAT_S32_ARGB;
    fill_config.src_para.left = 0;
    fill_config.src_para.top = 0;
    fill_config.src_para.width = VideoWindowWidth;
    fill_config.src_para.height = VideoWindowHeight; 
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
    fillRect.src1_rect.w = VideoWindowWidth;
    fillRect.src1_rect.h = VideoWindowHeight; 
    fillRect.color = 0x00000000;

    io = ioctl(ge2d_fd, GE2D_FILLRECTANGLE, &fillRect);
    if (io < 0)
    {
        printf("GE2D_FILLRECTANGLE failed.\n");
    }
#endif
}


void WaitVsync() {
   
	int fd = open("/dev/fb0", O_RDWR);
	
	if (ioctl(fd, FBIO_WAITFORVSYNC, 1) < 0)
	{
		printf("FBIO_WAITFORVSYNC failed.\n");
	}
	close(fd);
}	
	

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

///
/// Exit and cleanup video threads.
///
VideoThreadExit(void)
{
    if (VideoThread) {
        void *retval;

        Debug(3, "video: video thread canceled\n");

        // FIXME: can't cancel locked
        if (pthread_cancel(VideoThread)) {
            Debug(3, "video: can't queue cancel video display thread\n");
        }
        usleep(200000);                 // 200ms
        if (pthread_join(VideoThread, &retval) || retval != PTHREAD_CANCELED) {
            Debug(3, "video: can't cancel video decoder thread\n");
        }

       
        VideoThread = 0;
        //pthread_cond_destroy(&VideoWakeupCond);
        //pthread_mutex_destroy(&VideoLockMutex);
        //pthread_mutex_destroy(&VideoMutex);
        //pthread_mutex_destroy(&OSDMutex);

    }

}
void delete_decode()
{
    Debug(3, "decoder thread exit\n");
}


void *VideoDisplayHandlerThread(void *dummy)
{

  //  prctl(PR_SET_NAME, "video decoder", 0, 0, 0);
    
    pthread_cleanup_push(delete_decode, NULL);
    for (;;) {
        // fix dead-lock with OdroidExit
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        pthread_testcancel();
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

        OdroidDisplayHandlerThread();
    }
    pthread_cleanup_pop(NULL);
  
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
    pthread_create(&VideoThread, NULL, VideoDisplayHandlerThread, NULL);

 //   pthread_create(&VideoDisplayThread, NULL, VideoHandlerThread, NULL);

}

///
/// Handle a Odroid display.
///
int amlGetBufferFree(int);
void OdroidDisplayHandlerThread(void)
{
    int i;
    int err = 0;
    int free;
    int decoded;
    int64_t filled;
    struct timespec nowtime;
    OdroidDecoder *decoder;
	
	int64_t akt_pts, new_pts;

    decoded = 0;

	for (i = 0; i < OdroidDecoderN; ++i) { 
		filled = 0;
		decoder = OdroidDecoders[i];
		if (!decoder)
			continue;

		free = amlGetBufferFree(decoder->pip);	
		//printf("Free in Prozent: %d\n",free);

		if (free > 60) {
			// FIXME: hot polling
			// fetch+decode or reopen
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
	}
    
    usleep(5000);
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

	decoder->pip = 0;

	decoder->handle = -1;

    OdroidDecoders[OdroidDecoderN++] = decoder;

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
	int fd = open("/dev/amstream_vbuf", O_WRONLY);

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

bool getResolution(char *mode) {
	 int width = 0, height = 0, rrate = 60;
    char smode[2] = { 0 };

    if (sscanf(mode, "%dx%dp%dhz", &width, &height, &rrate) == 3)
    {
      *smode = 'p';
    }
    else if (sscanf(mode, "%d%1[ip]%dhz", &height, smode, &rrate) >= 2)
    {
      switch (height)
      {
        case 480:
        case 576:
          width = 720;
          break;
        case 720:
          width = 1280;
          break;
        case 1080:
          width = 1920;
          break;
        case 2160:
          width = 3840;
          break;
      }
    }
    else if (sscanf(mode, "%dcvbs", &height) == 1)
    {
      width = 720;
      *smode = 'i';
      rrate = (height == 576) ? 50 : 60;
    }
    else if (sscanf(mode, "4k2k%d", &rrate) == 1)
    {
      width = 3840;
      height = 2160;
      *smode = 'p';
    }
    else
    {
      return false;
    }
	VideoWindowWidth = width;
	VideoWindowHeight = height;
	return true;

}

// OSD dma_buf experimental support
#define FBIOGET_OSD_DMABUF               0x46fc


 void VideoInit(const char *i) 
{
	

	char mode[256];

	timeBase.num = 1;
	timeBase.den = 90000;

#if 0
	ge2d_fd = open("/dev/ge2d", O_RDWR);
    if (ge2d_fd < 0)
    {
        printf("open /dev/ge2d failed.");
    }
#endif

	// Control device
	cntl_handle = open(CODEC_CNTL_DEVICE, O_RDWR);
	if (cntl_handle < 0)
	{
		//codecMutex.Unlock();
		printf("open CODEC_CNTL_DEVICE failed.\n");
		return;
	}
	
	amlGetString("/sys/class/display/mode",mode,sizeof(mode));
	
	getResolution(mode);

	// enable alpha setting

	struct fb_var_screeninfo info;
	uint32_t h[3];

	fd = open("/dev/fb0", O_RDWR);
	ioctl(fd, FBIOGET_VSCREENINFO, &info);
	info.reserved[0] = 0;
	info.reserved[1] = 0;
	info.reserved[2] = 0;
	info.xoffset = 0;
	info.yoffset = 0;
	info.activate = FB_ACTIVATE_ALL;
	info.blue.offset = 0;
	info.blue.length = 8;
	info.green.offset = 8;
	info.green.length = 8;
	info.red.offset = 16;
	info.red.length = 8;
	info.transp.offset = 24;
	info.transp.length = 8;
	info.bits_per_pixel = 32;
	Debug(3,"Initial Screen %d-%d\n",info.xres,info.yres);
	info.xres = VideoWindowWidth-1;
	info.yres = VideoWindowHeight-1;
	info.xres_virtual = VideoWindowWidth-1;
	info.yres_virtual = (VideoWindowHeight*2)-1;
	info.nonstd = 1;
	ioctl(fd, FBIOPUT_VSCREENINFO, &info);

	if (ioctl(fd, FBIOGET_OSD_DMABUF, &h) < 0) {
	  	DmaBufferHandle = -1;
		Fatal("Unable to get DMABUF");
	} else {
		DmaBufferHandle = h[1];
	}
	
	close(fd);
#if 0
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
#endif
	if (VideoWindowWidth < 1920) {    // is screen is only 1280 or smaller
		OsdWidth = VideoWindowWidth;
		OsdHeight = VideoWindowHeight;
	} else {
		OsdWidth = 1920;
		OsdHeight = 1080;
	}

	// Check if H264-PIP is available
	int fd1 = open("/dev/amstream_vframe", O_WRONLY);
	int fd2 = open("/dev/amstream_vframe", O_WRONLY);  // can we open a second time
	
	if (fd2 > 0 ) {
		vformat_t amlFormat = (vformat_t)VFORMAT_MPEG12;
		char vfm_status[512];
		use_pip = 1;

		codec_h_ioctl_set(fd2,AMSTREAM_SET_VFORMAT,amlFormat);
		codec_h_ioctl_set(fd2,AMSTREAM_PORT_INIT,0);
		if (amlGetString("/sys/class/vfm/map",vfm_status,sizeof(vfm_status)) == 0) {
        	if (strstr(vfm_status,"mpeg12")) {
				use_pip_mpeg2 = 1;
			}
		}	
		close (fd2);
	}
	close(fd1);
	
	char fsaxis_str[256] = {0};
	char waxis_str[256] = {0};

	sprintf(fsaxis_str, "0 0 %d %d", OsdWidth-1, OsdHeight-1);
	sprintf(waxis_str, "0 0 %d %d", VideoWindowWidth-1, VideoWindowHeight-1);

	amlSetString("/sys/class/vfm/map","rm default");
	amlSetString("/sys/class/vfm/map","add default decoder ppmgr deinterlace amvideo");

	amlSetInt("/sys/class/graphics/fb0/free_scale", 0);
	amlSetString("/sys/class/graphics/fb0/free_scale_axis", fsaxis_str);
	amlSetString("/sys/class/graphics/fb0/window_axis", waxis_str);
	amlSetInt("/sys/class/graphics/fb0/scale_width", OsdWidth);
	amlSetInt("/sys/class/graphics/fb0/scale_height", OsdHeight);
	amlSetInt("/sys/class/graphics/fb0/free_scale", 0x10001);
	GetApiLevel();
	Debug(3,"aml ApiLevel = %d  Screen %d-%d using OSD dma: %s H264-PIP: %d MPEG2 PIP %d\n",apiLevel,VideoWindowWidth,VideoWindowHeight,(DmaBufferHandle >= 0) ? "yes": "no",use_pip,use_pip_mpeg2);
	ClearDisplay();
};


extern void DelPip(void);
///< Setup video module.
// Open video codec.
 void CodecVideoOpen(VideoDecoder *decoder, int codec_id, AVPacket *avpkt)
 {
	int pip = decoder->HwDecoder->pip;
	switch (codec_id)
	{
	case AV_CODEC_ID_MPEG2VIDEO:
		videoFormat = Mpeg2;
		if (!use_pip_mpeg2 && !pip && isPIP) {
			InternalClose(1);
		}
		break;

	case AV_CODEC_ID_H264:
			videoFormat = Avc;
		break;

	case AV_CODEC_ID_HEVC:
		videoFormat = Hevc;
		if (use_pip && !pip && isPIP) {
			InternalClose(1);
		}
		break;

	default:
			printf("Unknown Video Codec\n");
		
			return;
		
	}
	
	if (!pip) {
		FirstVPTS = AV_NOPTS_VALUE;
		isAnnexB = false;
		isFirstVideoPacket = true;
	}

	if (isOpen && !pip) {
		InternalClose(pip);
	}
	
	InternalOpen (decoder->HwDecoder,videoFormat, FrameRate);
	if (!pip) {
		//SetCurrentPCR(decoder->HwDecoder->handle,avpkt->pts);
		amlResume();
		if (!isPIP)
			amlSetVideoAxis(0, VideoWindowX,VideoWindowY, VideoWindowWidth, VideoWindowHeight);
	}
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
	case AMSTREAM_SET_FRAME_BASE_PATH:
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
        //printf("codec_h_ioctl_set failed,handle=%d,cmd=%x,paramter=%x, t=%x errno=%d\n", h, subcmd, paramter, r, errno);
        return r;
    }
    return 0;
}

const char* CODEC_VIDEO_ES_DEVICE = "/dev/amstream_vbuf";
const char* CODEC_VIDEO_ES_HEVC_DEVICE = "/dev/amstream_hevc";
const char* CODEC_VIDEO_ES_DEVICE_SCHED = "/dev/amstream_vframe";
const char* CODEC_VIDEO_ES_HEVC_DEVICE_SCHED = "/dev/amstream_hevc_sched";


void InternalOpen(VideoHwDecoder *hwdecoder, int format, double frameRate)
{
    
	int handle;
	int pip = hwdecoder->pip;
	// Open codec
	int flags = O_WRONLY;
	vformat_t amlFormat = (vformat_t)0;
	dec_sysinfo_t am_sysinfo = { 0 };

	if (!pip) {
		PIP_allowed = false;
	}

	if (!pip)
 	    amlSetInt("/sys/class/video/disable_video",0);
	
	switch (format)
	{
		case Hevc:
			hwdecoder->handle = open("/dev/amstream_hevc", flags);
			Debug(3,"AmlVideoSink - VIDEO/HEVC\n");

			amlFormat = VFORMAT_HEVC;
			am_sysinfo.format = VIDEO_DEC_FORMAT_HEVC;
			if (use_pip && !pip && isPIP) {
				DelPip();
			}
			break;
		case Avc:
			
			Debug(3,"AmlVideoSink - VIDEO/H264\n");

			amlFormat = VFORMAT_H264;
			am_sysinfo.format = VIDEO_DEC_FORMAT_H264;
			if (use_pip) {
				PIP_allowed = true;
			}
			hwdecoder->handle = open("/dev/amstream_vframe", flags);
			

			break;
		case Mpeg2:
			Debug(3,"AmlVideoSink - VIDEO/MPEG2\n");
			amlFormat = VFORMAT_MPEG12;
			am_sysinfo.format = VIDEO_DEC_FORMAT_UNKNOW;
			if (use_pip_mpeg2) {
				PIP_allowed = true;
				hwdecoder->handle = open("/dev/amstream_vframe", flags);
			} else {
				hwdecoder->handle = open("/dev/amstream_vbuf", flags);
				if (use_pip && !pip && isPIP) {
					DelPip();
				}		
			}
			break;
		default:
			Debug(3,"AmlVideoSink - VIDEO/UNKNOWN(%d)\n", (int)format);
            return;
			
	}

	if (hwdecoder->handle < 0)
	{	
		Debug(3,"AmlCodec open failed. %d\n",hwdecoder->handle);
        return;
	}
	handle = hwdecoder->handle;
	hwdecoder->Format = format;

	// Set video format
	
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

#if 0
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
#endif
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
		
	if (apiLevel >= S905) // S905
	{
		if (!pip) {
			//codec_h_ioctl_set(handle,AMSTREAM_SET_FRAME_BASE_PATH,FRAME_BASE_PATH_TUNNEL_MODE);
			if (format == Avc) {
				amlSetString("/sys/class/vfm/map","rm vdec-map-0");
				amlSetString("/sys/class/vfm/map","add pip0 vdec.h264.00  ppmgr deinterlace amvideo");
			}
			else if (use_pip_mpeg2 && (format == Mpeg2)) {
				amlSetString("/sys/class/vfm/map","add pip0 vdec.mpeg12.00 ppmgr deinterlace  amvideo");
			}				
		}
		//amlSetInt("/sys/class/video/blackout_policy", 0);


		if (use_pip && PIP_allowed && pip) {
			isPIP = true;
		    if (format == Avc) {
			   //codec_h_ioctl_set(handle,AMSTREAM_SET_FRAME_BASE_PATH,FRAME_BASE_PATH_AMLVIDEO1_AMVIDEO2);
		       amlSetString("/sys/class/vfm/map","add pip1 vdec.h264.01 videopip");
			}
			else {
			   amlSetString("/sys/class/vfm/map","add pip1 vdec.mpeg12.01 videopip");
			}
			amlSetInt("/sys/class/video/pip_global_output",1);
		}
	}
	
	r = ioctl(handle, AMSTREAM_IOC_SYSINFO, (unsigned long)&am_sysinfo);
	if (r < 0)
	{
		//codecMutex.Unlock();
		printf("AMSTREAM_IOC_SYSINFO failed.\n");
        return;
	}

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
	if (!pip) {
		r = ioctl(cntl_handle, AMSTREAM_IOC_SYNCENABLE, (unsigned long)1);
		if (r != 0)
		{
			//codecMutex.Unlock();
			printf("AMSTREAM_IOC_SYNCENABLE failed.\n");
			return;
		}
	}

#if 0
	// Restore settings that Kodi tramples
	r = ioctl(cntl_handle, AMSTREAM_IOC_SET_VIDEO_DISABLE, (unsigned long)VIDEO_DISABLE_NONE);
	if (r != 0)
	{
		//printf("AMSTREAM_IOC_SET_VIDEO_DISABLE VIDEO_DISABLE_NONE failed.\n");
	}

#endif
	//amlSetInt("/sys/class/video/disable_video",0);
	
	uint32_t screenMode = (uint32_t)VIDEO_WIDEOPTION_16_9;
	if (!pip) {
		r = ioctl(cntl_handle, AMSTREAM_IOC_SET_SCREEN_MODE, &screenMode);
	} else {
		r = ioctl(cntl_handle, AMSTREAM_IOC_SET_PIP_SCREEN_MODE, &screenMode);
		//screenMode = (uint32_t) VIDEO_DISABLE_NONE;
		//r |= ioctl(cntl_handle, AMSTREAM_IOC_SET_VIDEOPIP_DISABLE, &screenMode);
	}
	if (r != 0)
	{
		printf("AMSTREAM_IOC_SET_SCREEN_MODE VIDEO_WIDEOPTION_NORMAL failed");
		return;
	}
#if 0
	if (pip) {
		int res;
		ioctl(cntl_handle, AMSTREAM_IOC_GLOBAL_GET_VIDEO_OUTPUT, &res);
		printf("Videooutput %d\n",res);
		ioctl(cntl_handle, AMSTREAM_IOC_GLOBAL_SET_VIDEOPIP_OUTPUT, 1);
		ioctl(cntl_handle, AMSTREAM_IOC_GLOBAL_GET_VIDEOPIP_OUTPUT, &res);
		printf("VideoPIPoutput %d\n",res);
		ioctl(cntl_handle, AMSTREAM_IOC_GET_PIP_ZORDER, &res);
		printf("VideoPIPZorder %d\n",res);
	}
#endif

	isOpen = true;
}

uint64_t GetCurrentVPts(int handle)
{
	//codecMutex.Lock();

	if (!isOpen)
	{
		//codecMutex.Unlock();
		printf("The codec is not open.");
		return 0;
	}

    handle = OdroidDecoders[0]->handle;

	uint64_t vpts;
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
			printf("AMSTREAM_GET_VPTS failed.\n");
			return 0;
		}

		vpts = parm.data_32;
		//vpts = parm.data_64;

		
	}
	else	// S805
	{
		ret = ioctl(handle, AMSTREAM_IOC_VPTS, (unsigned long)&vpts);
		if (ret < 0)
		{
			//codecMutex.Unlock();
			printf("AMSTREAM_IOC_VPTS failed.\n");
			return 0;
		}
	}

	//codecMutex.Unlock();

	return vpts; // / (double)PTS_FREQ;
}

uint64_t GetCurrentAPts(int handle)
{
	//codecMutex.Lock();

	if (!isOpen)
	{
		//codecMutex.Unlock();
		printf("The codec is not open.");
		return 0;
	}


	uint64_t vpts;
	int ret;
	if (apiLevel >= S905)	// S905
	{
		//int vpts = codec_get_vpts(&codec);
		//unsigned int vpts;
		struct am_ioctl_parm parm = { 0 };

		parm.cmd = AMSTREAM_GET_APTS;
		//parm.data_32 = &vpts;

		ret = ioctl(handle, AMSTREAM_IOC_GET, (unsigned long)&parm);
		if (ret < 0)
		{
			//codecMutex.Unlock();
			printf("AMSTREAM_GET_APTS failed.");
			return 0;
		}

		//vpts = parm.data_32;
		vpts = parm.data_64;

	}
	else	// S805
	{
		ret = ioctl(handle, AMSTREAM_IOC_APTS, (unsigned long)&vpts);
		if (ret < 0)
		{
			//codecMutex.Unlock();
			printf("AMSTREAM_IOC_APTS failed.");
			return 0;
		}
	}

	//codecMutex.Unlock();

	return vpts; // / (double)PTS_FREQ;
}

int SetCurrentPCR(int handle, uint64_t value)
{
	// codecMutex.Lock();

	if (!isOpen)
	{
		//codecMutex.Unlock();
		//printf("The codec is not open.\n");
		return 2;
	}

	handle = OdroidDecoders[0]->handle;
	if (handle == -1) {
		Debug(3,"SetPCR Invalide handle");
		return 1;
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

	//Debug(3,"set SCR %04lx",pts);


	if (apiLevel >= S905)	// S905
	{
		int ret = codec_h_ioctl_set(handle,AMSTREAM_SET_PCRSCR,pts);
		if (ret < 0)
		{
			//codecMutex.Unlock();
			//printf("AMSTREAM_IOC_SET_PCRSCR failed.\n");
			return 2;
		}
	}
	else	// S805
	{
		int ret = ioctl(handle, AMSTREAM_IOC_SET_PCRSCR, pts);
		if (ret < 0)
		{
			//codecMutex.Unlock();
			//printf("AMSTREAM_IOC_SET_PCRSCR failed.\n");
			return 2;
		}
	}

	return 0;

	//codecMutex.Unlock();
}


void ProcessBuffer(VideoHwDecoder *hwdecoder, AVPacket* pkt)
{
	//playPauseMutex.Lock();
	static int ratio=3;
	uint32_t screenMode;
	int len,b2;
	int pip = hwdecoder->pip;
	if (doResumeFlag)
	{
		//codec_resume(&codecContext);
		amlResume();

		doResumeFlag = false;
	}


	unsigned char* nalHeader = (unsigned char*)pkt->data;
	len = pkt->size - 4;

	if (isFirstVideoPacket)
	{
#if 0
		printf("Header (pkt.size=%x):\n", pkt->size);
		for (int j = 0; j < 16; ++j)	//nalHeaderLength
		{
			printf("%02x ", nalHeader[j]);
		}
		printf("\n");
		printf("PTS %04lx\n",pkt->pts);
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

#if 1		
		switch(hwdecoder->Format) {
			case Hevc:
						{
							int nal_unit_type;
							nalHeader = (unsigned char*)pkt->data;
							while (len--) {
								if (nalHeader[0] == 0 && 
									nalHeader[1] == 0 &&
									nalHeader[2] == 1) {
										nal_unit_type = ((nalHeader[3] >>1)  & 0x3f);  		// Get Frame Type
										//printf("HEVC Got Unit Type %d (%02x)\n",nal_unit_type,nalHeader[3]);
										if (nal_unit_type == 32 ) {
											break;
											
										}
										else {
											nalHeader++;
											continue;
										}
								}
								else {
									nalHeader++;										// wait for I-Frame
								}
							}
							if (len <= 0) {
								//printf("No I-Frame found PTS:%04lx len %d (%d) -> %d\n",pkt->pts,len,pkt->size,nal_unit_type);
								return;
							}
							else {
								//printf("H265 Unit Type %d  PTS %04lx\n",nal_unit_type,pkt->pts);
							}
						}
						break;
			case Avc:
						{
							int nal_unit_type;
							nalHeader = (unsigned char*)pkt->data;
							while (len--) {
								if (nalHeader[0] == 0 && 
									nalHeader[1] == 0 &&
									nalHeader[2] == 1) {
										nal_unit_type = (nalHeader[3] & 0x1f);  		// Get Frame Type
										//printf("Got Unit Type %d (%02x)\n",nal_unit_type,nalHeader[3]);
										if (nal_unit_type == 5 || nal_unit_type == 7 || nal_unit_type == 8) {
											break;
											
										}
										else {
											nalHeader++;
											continue;
										}
								}
								else {
									nalHeader++;										// wait for I-Frame
								}
							}
							if (len <= 0) {
								//printf("No I-Frame found PTS:%04lx len %d (%d) -> %d\n",pkt->pts,len,pkt->size,nal_unit_type);
								return;
							}
							else {
								//printf("H264 Unit Type %d  PTS %04lx\n",nal_unit_type,pkt->pts);
							}
						}
						break;
			case Mpeg2:
				while (len--) {
					if (nalHeader[0] == 0 && 
						nalHeader[1] == 0 &&
						nalHeader[2] == 1 &&
						nalHeader[3] == 0) {						// Picture Start Code
							b2 = (nalHeader[5] >> 3) & 0x07;  		// Get Frame Type
							//printf("Got Frame Type %d\n",b2);
							if (b2 != 1) {
								return;
							}
							else {
								//printf("2.PTS %04lx\n",pkt->pts);
								break;
							}
					}
					else {
						nalHeader++;										// wait for I-Frame
					}
				}
				if (len <= 0) {
					//printf("No I-Frame found PTS:%04lx len %d (%d) -> %d\n",pkt->pts,len,pkt->size,b2);
					return;
				}
				else
					//printf("Found I-Frame len %d (%d) b2 %d\n\n",len,pkt->size,b2);
				break;
			default:
				break;
		}
#endif		
		if (!pip) {
			FirstVPTS = pkt->pts;
			Debug(3,"first vpts: %#012" PRIx64 "\n",FirstVPTS & 0xffffffff);
			uint64_t dpts = pkt->pts & 0xffffffff;
			SetCurrentPCR(hwdecoder->handle,dpts);
		}

		//amlCodec.SetSyncThreshold(pts);

		isFirstVideoPacket = false;
		
	}
	else {
		if (hwdecoder->Format == Mpeg2 && !pip &&
			nalHeader[0] == 0 && 
			nalHeader[1] == 0 &&
			nalHeader[2] == 1 &&
			nalHeader[3] == 0xb3) {						// Sequence Header
				int r = (nalHeader[7] >> 4) & 0x0f;  		// Get Ratio
				//printf("Got Ratio %d\n",r);
				if (r == 2 && r != ratio) {
					//printf("Set Ratio %d\n",r);
					ratio = 2;
					screenMode = (uint32_t)VIDEO_WIDEOPTION_4_3; 
					ioctl(cntl_handle, AMSTREAM_IOC_SET_SCREEN_MODE, &screenMode);
				} else if (r == 3 && r != ratio) {
					//printf("Set Ratio %d\n",r);
					ratio = 3;
					screenMode = (uint32_t)VIDEO_WIDEOPTION_16_9; 
					ioctl(cntl_handle, AMSTREAM_IOC_SET_SCREEN_MODE, &screenMode);
				}
			}
	}

#if 0	
nalHeader = (unsigned char*)pkt->data;
	len = pkt->size - 4;
	if (test > 0) {
	switch(hwdecoder->Format) {
			case Hevc:
						{
							int nal_unit_type;
							
							while (len--) {
								if (nalHeader[0] == 0 && 
									nalHeader[1] == 0 &&
									nalHeader[2] == 1) {
										nal_unit_type = ((nalHeader[3] >> 1)  & 0x3f);  		// Get Frame Type
										printf("HEVC Got Unit Type %d (%02x)\n",nal_unit_type,nalHeader[3]);
										if (nal_unit_type != 0x20) {
											nalHeader++;
											continue;
										}
										else {
											break;
										}
								}
								else {
									nalHeader++;										// wait for I-Frame
								}
							}
							if (len <= 0) {
								printf("No I-Frame found PTS:%04lx len %d (%d) -> %d\n",pkt->pts,len,pkt->size,nal_unit_type);
								//return;
							}
							else {
								printf("H265 Unit Type %d  PTS %04lx\n",nal_unit_type,pkt->pts);
							}
						}
						break;
			default:
				break;

	}
	test--;
	}
#endif

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
			SendCodecData(pip,pts, pkt->data, pkt->size);
			break;
		}

		case Mpeg4:
		{
			//unsigned char* video_extra_data = &extraData[0];
			//int video_extra_data_size = extraData.size();

			//SendCodecData(0, video_extra_data, video_extra_data_size);

			printf("Missing extra Data in mpeg4\n");

			SendCodecData(pip, pts, pkt->data, pkt->size);

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

			if (!SendCodecData(pip, pts, pkt->data, pkt->size))
			{
				// Resend extra data on codec reset
				isExtraDataSent = false;

				printf("AmlVideoSinkElement::ProcessBuffer - SendData Failed. pip %d\n",pip);
			}

			break;
		}

		case VC1:
		{
			SendCodecData(pip, pts, pkt->data, pkt->size);

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

void CheckinPts(int handle, uint64_t pts)
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
		codec_h_ioctl_set(handle,AMSTREAM_SET_TSTAMP,(unsigned long)pts);
	}
	else	// S805
	{
		int r = ioctl(handle, AMSTREAM_IOC_TSTAMP, (unsigned long) pts);
		if (r < 0)
		{
			//codecMutex.Unlock();
			printf("AMSTREAM_IOC_TSTAMP failed\n");
			return;
		}
	}

	//codecMutex.Unlock();
}

int WriteData(int handle, unsigned char* data, int length)
{
	if (data == NULL) {
		return;
	}
	if (length < 1) {
		return;
	}
	
	int ret = write(handle, data, length);
	if (ret == length) {
		usleep(2000);
	}
	
	return ret; //written;
}

Bool SendCodecData(int pip, uint64_t pts, unsigned char* data, int length)
{
	//printf("AmlVideoSink: SendCodecData - pts=%lu, data=%p, length=0x%x\n", pts, data, length);
	Bool result = true;

    int handle = OdroidDecoders[pip]->handle;

	if ((pts > 0) && !pip)
	{
		LastPTS = pts;
		CheckinPts(handle, pts);
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

		int count = WriteData(handle, data + offset, length - offset);
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
				if (!pip)
					amlReset();
				else
					Debug(3,"PIP needs reset");
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


amlReset()
{
	// codecMutex.Lock();
	Debug(3,"amlreset");
	if (!isOpen)
	{
		//codecMutex.Unlock();
		//Debug(3,"The codec is not open. %s\n",__FUNCTION__);
		return;
	}
	// set the system blackout_policy to leave the last frame showing
	int blackout_policy;
	amlGetInt("/sys/class/video/blackout_policy", &blackout_policy);
	amlSetInt("/sys/class/video/blackout_policy", 0);

	//amlPause();
;
	InternalClose(0);
	FirstVPTS = AV_NOPTS_VALUE;
	isFirstVideoPacket = true;
	InternalOpen(OdroidDecoders[0], videoFormat,FrameRate);

	//amlSetVideoDelayLimit(1000);

	amlSetInt("/sys/class/video/blackout_policy", blackout_policy);
	
	//printf("amlReset\n");
	
	//codecMutex.Unlock();
}

void InternalClose(int pip)
{
	int r;
	int handle = OdroidDecoders[pip]->handle;
	if (handle == -1 || handle == 0) {
		Debug(3,"Internal Close mit Handle %d\n",handle);
		return;
	}
	//amlClearVideo();

	r = close(handle);
	if (r < 0)
	{
		//codecMutex.Unlock();
		printf("close handle failed PIP %d\n.",pip);
		return;
	}
	if (pip) {
		
		uint32_t nMode = 1;
		ioctl(cntl_handle, AMSTREAM_IOC_SET_VIDEOPIP_DISABLE, &nMode);
		amlSetString("/sys/class/vfm/map","rm pip1");
		amlSetString("/sys/class/vfm/map","rm vdec-map-1");
		amlSetInt("/sys/class/video/pip_global_output",0);
	} else {
		amlSetString("/sys/class/vfm/map","rm pip0");
	}
	

	OdroidDecoders[pip]->handle = -1;

	if (!pip) {
		isOpen = false;
	}

	Debug(3,"internal close pip %d",pip);
}

void amlSetVideoAxis(int pip, int x, int y, int width, int height)
{
	//codecMutex.Lock();
	int ret;
	//printf("scale video Pip %d  %d:%d-%d:%d\n",pip,x,y,width,height);
	if (!isOpen)
	{
		//codecMutex.Unlock();
		printf("The codec is not open. %s\n",__FUNCTION__);
		return;
	}

	int params[4] = { x, y, width-1, height-1 };
	if (!pip) {
		ret = ioctl(cntl_handle, AMSTREAM_IOC_SET_VIDEO_AXIS, &params);
	} else {
		ret = ioctl(cntl_handle, AMSTREAM_IOC_SET_VIDEOPIP_AXIS, &params);
	}

	//codecMutex.Unlock();

	if (ret < 0)
	{
		printf("AMSTREAM_IOC_SET_VIDEO_AXIS failed.");
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

void amlTrickMode(int val)  // unused
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

int amlGetBufferFree(int pip)  
{
	//codecMutex.Lock();

	if (!isOpen)
	{
		//codecMutex.Unlock();
		//printf("The codec is not open. %s\n",__FUNCTION__);
		return 100;
	}
	int handle = OdroidDecoders[pip]->handle;
	
	struct buf_status status;
	if (apiLevel >= S905)	// S905
	{
		struct am_ioctl_parm_ex parm = { 0 };
		parm.cmd = AMSTREAM_GET_EX_VB_STATUS;
		
		int r = ioctl(handle, AMSTREAM_IOC_GET_EX, (unsigned long)&parm);

		//codecMutex.Unlock();

		if (r < 0)
		{
			//printf("AMSTREAM_GET_EX_VB_STATUS failed.\n");
			return 100;
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
			//printf("AMSTREAM_IOC_VB_STATUS failed.\n");
			return 100;
		}

		memcpy(&status, &am_io.status, sizeof(status));
	}
	//printf("STatus: write %u read %u free %d size %d data %d\n",status.write_pointer,status.read_pointer,status.free_len,status.size,status.data_len);
	if (status.size)
		return (status.free_len * 100) / status.size;
	else
		return 0;
}


int amlSetString(char *path, char *valstr)
{
  int fd = open(path, O_WRONLY, 0644);
  int ret = 0;
  if (fd >= 0)
  {
    if (write(fd, valstr, strlen(valstr)) < 0)
      ret = -1;
    close(fd);
  }
  if (ret)
    Debug(3, "%s: error writing %s",__FUNCTION__, path);

  return ret;
}

int amlGetString(char *path, char *valstr, size_t size)
{
  int len;

  int fd = open(path, O_RDONLY);
  if (fd >= 0)
  {
    len = read(fd, valstr, size);
    close(fd);
    if (len > 0) {
       return 0;
    }
  }
  Debug(3, "%s: error reading %s",__FUNCTION__, path);
  if (valstr)
     *valstr = 0;
  return -1;
}

int amlSetInt(char *path, int val)
{
  int fd = open(path, O_WRONLY, 0644);
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




#if 0
void amlClearVideo()  
{
	
	Debug(3,"clear vbuf\n");
	if (!isOpen)
	{
		printf("The codec is not open. %s\n",__FUNCTION__);
		return;
	}
	amlSetInt("/sys/class/video/blackout_policy", 1);
	int r = ioctl(cntl_handle,AMSTREAM_IOC_CLEAR_VIDEO  ,1);
	amlSetInt("/sys/class/video/blackout_policy", 0);
	if (r < 0)
	{
		printf("AMSTREAM_CLEAR_VIDEO failed.");
		return;
	}	
}

void amlClearVBuf()   // unused
{
	//codecMutex.Lock();

	int handle = OdroidDecoders[0]->handle;

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

void amlSetVideoDelayLimit(int ms)  // unused
{
	
    codec_h_ioctl_set (handle, AMSTREAM_SET_VIDEO_DELAY_LIMIT_MS, ms);

}

void amlDecReset()  // unused
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


amlClearVideo()  // unused
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
    //printf("clear video\n");
	//codecMutex.Unlock();
}

void amlGetVideoAxis()  // unused
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

#endif
