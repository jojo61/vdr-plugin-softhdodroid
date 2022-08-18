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
    o Audio Amp control via CEC
    o HDMI/SPDIF pass-through
    o Suspend / Dettach 


This is a Device driver for Odroid-n2(plus) Hardware.  SD, HD and UHD is working.

Good luck
jojo61
   
    
Install:
--------
	Preparation:

	I use ubuntu 20.04 for Odroid-n2 with kernel 4.9 from here:	https://odroid.in/ubuntu_20.04lts/n2/

	In order to compile the vdr you need to prepare the following.
	# apt build-dep vdr
	# apt install libgl-dev libglu-dev libgles2-mesa-dev freeglut3-dev libglm-dev libavcodec-dev libdrm-dev libasound2-dev vdr-dev
	
	Install from git

	git clone git://github.com/jojo61/vdr-plugin-softhdodroid.git
	cd vdr-plugin-softhdodroid
	make
	make install

	You have to start vdr with e.g.:  -P 'softhdodroid -a hw:CARD=AMLAUGESOUND,DEV=0 ..<more options>.. '

	You need to run vdr as root.
	You should adapt config.ini in /media/boot and change the Screensettings from hdmimode=1080p60hz to your preferred 
	Resolution. Also set display_autodetect=false

	If you want to have 10Bit colordepth you should insert something like that in rc.local:
	echo 420,10bit >/sys/class/amhdmitx/amhdmitx0/attr
	echo 2160p50hz >/sys/class/display/mode
	You need a UHD Display for this. All FullHD Displays are only 8 Bit.



 


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

    



Commandline:
------------

	Use vdr -h to see the command line arguments supported by the plugin.

    -a audio_device



	


SVDRP:
------

	Use 'svdrpsend.pl plug softhddevice HELP'
	or 'svdrpsend plug softhddevice HELP' to see the SVDRP commands help
	and which are supported by the plugin.

Keymacros:
----------

	See keymacros.conf how to setup the macros.

	This are the supported key sequences:

	@softhdodroid Blue 1 0		disable pass-through
	@softhdodroid Blue 1 1		enable pass-through
	@softhdodroid Blue 1 2		toggle pass-through
	@softhdodroid Blue 1 3		decrease audio delay by 10ms
	@softhdodroid Blue 1 4		increase audio delay by 10ms
	@softhdodroid Blue 1 5		toggle ac3 mixdown
	

