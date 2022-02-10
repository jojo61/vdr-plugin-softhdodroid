///
/// @file codec.h   @brief Codec module headerfile
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
/// $Id: bdb4d18dbe371e497d039e45faa7c134b019860a $
//////////////////////////////////////////////////////////////////////////////

/// @addtogroup Codec
/// @{

//----------------------------------------------------------------------------
//  Defines
//----------------------------------------------------------------------------

#define CodecPCM 0x01                   ///< PCM bit mask
#define CodecMPA 0x02                   ///< MPA bit mask (planned)
#define CodecAC3 0x04                   ///< AC-3 bit mask
#define CodecEAC3 0x08                  ///< E-AC-3 bit mask
#define CodecDTS 0x10                   ///< DTS bit mask (planned)

#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000


//----------------------------------------------------------------------------
//  Variables
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
//  Variables
//----------------------------------------------------------------------------

/// Flag prefer fast xhannel switch
extern char CodecUsePossibleDefectFrames;

//----------------------------------------------------------------------------
//  Prototypes
//----------------------------------------------------------------------------

/// Allocate a new video decoder context.
extern VideoDecoder *CodecVideoNewDecoder(VideoHwDecoder *);

/// Deallocate a video decoder context.
extern void CodecVideoDelDecoder(VideoDecoder *);

/// Open video codec.
extern void CodecVideoOpen(VideoDecoder *, int, AVPacket *);

/// Close video codec.
extern void CodecVideoClose(VideoHwDecoder *);

/// Flush video buffers.
extern void CodecVideoFlushBuffers(VideoDecoder *);

/// Allocate a new audio decoder context.
extern AudioDecoder *CodecAudioNewDecoder(void);

/// Deallocate an audio decoder context.
extern void CodecAudioDelDecoder(AudioDecoder *);

/// Open audio codec.
extern void CodecAudioOpen(AudioDecoder *, int);

/// Close audio codec.
extern void CodecAudioClose(AudioDecoder *);

/// Set audio drift correction.
extern void CodecSetAudioDrift(int);

/// Set audio pass-through.
extern void CodecSetAudioPassthrough(int);

/// Set audio downmix.
extern void CodecSetAudioDownmix(int);

/// Flush audio buffers.
extern void CodecAudioFlushBuffers(AudioDecoder *);

/// Setup and initialize codec module.
extern void CodecInit(void);

/// Cleanup and exit codec module.
extern void CodecExit(void);

/// @}
