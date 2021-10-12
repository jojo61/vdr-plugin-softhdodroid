@file README.txt		@brief A software UHD output device for VDR


Copyright (c) 2021 by jojo61.  All Rights Reserved.

Contributor(s):

jojo61

License: AGPLv3

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as
published by the Free Software Foundation, either version 3 of the
License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

A software and GPU emulated UHD output device plugin for VDR.

    o Video hardware decoder ODROID
    o Audio FFMpeg / Alsa / Analog
    o Audio FFMpeg / Alsa / Digital
    o Audio FFMpeg / OSS / Analog
    o HDMI/SPDIF pass-through
    o Suspend / Dettach  (not working yet)


This is a Device driver for Odroid-n2(plus) Hardware.  SD, HD and UHD is working.

Current Status:
The driver supports SD, HD and UHD Streams. So far I have only tested with SD and HD.
Video editing is possible but not perfect yet. I found no way to clear the Video Buffer
and so I have to Reset the Device befor showing an I-Frame. This resuts in a short black
screen.


Good luck
jojo61
   
    
Install:
--------
	Install from git

	git clone git://github.com/jojo61/vdr-plugin-softhdodroid.git
	cd vdr-plugin-softhdodroid
	make
	make install

	You have to start vdr with e.g.:  -P 'softhdodroid -a default -r 50  ..<more options>.. '

 


Setup:	environment
------
	Following is supported:

	
    
	ALSA_DEVICE=default
		alsa PCM device name
	ALSA_PASSTHROUGH_DEVICE=
		alsa pass-though (AC-3,E-AC-3,DTS,...) device name
	ALSA_MIXER=default
		alsa control device name
	ALSA_MIXER_CHANNEL=PCM
		alsa control channel name

    
	OSS_AUDIODEV=/dev/dsp
		oss dsp device name
	OSS_PASSTHROUGHDEV=
		oss pass-though (AC-3,E-AC-3,DTS,...) device name
	OSS_MIXERDEV=/dev/mixer
		oss mixer device name
	OSS_MIXER_CHANNEL=pcm
		oss mixer channel name


Commandline:
------------

	Use vdr -h to see the command line arguments supported by the plugin.

    -a audio_device

	Selects audio output module and device.
	""		to disable audio output
	/...		to use oss audio module (if compiled with oss
			support)
	other		to use alsa audio module (if compiled with alsa
			support)

SVDRP:
------

	Use 'svdrpsend.pl plug softhddevice HELP'
	or 'svdrpsend plug softhddevice HELP' to see the SVDRP commands help
	and which are supported by the plugin.

Keymacros:
----------

	See keymacros.conf how to setup the macros.

	This are the supported key sequences:

	@softhdcuvid Blue 1 0		disable pass-through
	@softhdcuvid Blue 1 1		enable pass-through
	@softhdcuvid Blue 1 2		toggle pass-through
	@softhdcuvid Blue 1 3		decrease audio delay by 10ms
	@softhdcuvid Blue 1 4		increase audio delay by 10ms
	@softhdcuvid Blue 1 5		toggle ac3 mixdown
	

