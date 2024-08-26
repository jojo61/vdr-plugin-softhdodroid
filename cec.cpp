/*
 * This file is part of the libCEC(R) library.
 *
 * libCEC(R) is Copyright (C) 2011-2015 Pulse-Eight Limited.  All rights reserved.
 * libCEC(R) is an original work, containing original code.
 *
 * libCEC(R) is a trademark of Pulse-Eight Limited.
 *
 * This program is dual-licensed; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 *
 *
 * Alternatively, you can license this library under a commercial license,
 * please contact Pulse-Eight Licensing for more information.
 *
 * For more information contact:
 * Pulse-Eight Licensing       <license@pulse-eight.com>
 *     http://www.pulse-eight.com/
 *     http://www.pulse-eight.net/
 */


#include "cec.h"

#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <signal.h>
#include <stdlib.h>
#include "p8-platform/os.h"
#include "p8-platform/util/StringUtils.h"
#include "p8-platform/threads/threads.h"


extern "C" {
    int cec_init();
    int cec_exit();
    int ProcessCommandTX(char *);
}

using namespace CEC;
using namespace P8PLATFORM;

#include "cecloader.h"

#if CEC_LIB_VERSION_MAJOR >= 5
#define LIBCEC_OSD_NAME_SIZE (15)
#else
#define LIBCEC_OSD_NAME_SIZE (13)
#endif

ICECCallbacks         g_callbacks;
libcec_configuration  g_config;
int                   g_cecLogLevel(-1);
int                   g_cecDefaultLogLevel(CEC_LOG_ERROR);
std::string           g_strPort;
ICECAdapter*          g_parser;

int ProcessCommandTX(char *arguments)
{
  
    std::string strvalue;
    cec_command bytes = g_parser->CommandFromString(arguments);

    bytes.ack = 0;
    bytes.eom = 0;
    bytes.transmit_timeout = 1000;
    
    g_parser->Transmit(bytes);

    return 0;
}

int cec_init() {

    g_config.Clear();
    g_callbacks.Clear();
    snprintf(g_config.strDeviceName, LIBCEC_OSD_NAME_SIZE, "VDR CEC");
    g_config.clientVersion      = LIBCEC_VERSION_CURRENT;
    g_config.bActivateSource    = 0;
    g_callbacks.logMessage      = NULL;
    g_callbacks.keyPress        = NULL;
    g_callbacks.commandReceived = NULL;
    g_callbacks.alert           = NULL;
    g_config.callbacks          = NULL;

    g_config.deviceTypes.Add(CEC_DEVICE_TYPE_RECORDING_DEVICE);

    if (g_cecLogLevel == -1)
        g_cecLogLevel = g_cecDefaultLogLevel;


    g_parser = LibCecInitialise(&g_config);
    if (!g_parser)
    {
    
        std::cout << "Cannot load libcec.so" << std::endl;

        if (g_parser)
            UnloadLibCec(g_parser);

        return 0;
    }

    g_strPort = "AOCEC";

    if (!g_parser->Open(g_strPort.c_str()))
    {
        UnloadLibCec(g_parser);
        return 0;
    }
    
    return 1;
}

int cec_exit() {
    if (g_parser)
        UnloadLibCec(g_parser);
    return 0;
}
