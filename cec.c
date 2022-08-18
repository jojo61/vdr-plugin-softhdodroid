  
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

#include "cectypes.h"

#ifdef UNUSED
#elif defined(__GNUC__)
#define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#elif defined(__LCLINT__)
#define UNUSED(x) /*@unused@*/ x
#else
#define UNUSED(x) x
#endif

#include "ceccloader.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>

static void cb_cec_log_message(void* lib, const cec_log_message* message);

static ICECCallbacks        g_callbacks = {
    .logMessage           = cb_cec_log_message,
    .keyPress             = NULL,
    .commandReceived      = NULL,
    .configurationChanged = NULL,
    .alert                = NULL,
    .menuStateChanged     = NULL,
    .sourceActivated      = NULL
};

static libcec_configuration  g_config;
static int                   g_cecLogLevel = -1;
static int                   g_cecDefaultLogLevel = 0; //CEC_LOG_ALL;
static char                  g_strPort[50] = { 0 };
static int                   g_bSingleCommand = 0;
static volatile sig_atomic_t g_bExit = 0;
static libcec_interface_t    g_iface;

static void cb_cec_log_message(void* lib, const cec_log_message* message)
{
  if ((message->level & g_cecLogLevel) == message->level)
  {
    const char* strLevel;
    switch (message->level)
    {
    case CEC_LOG_ERROR:
      strLevel = "ERROR:   ";
      break;
    case CEC_LOG_WARNING:
      strLevel = "WARNING: ";
      break;
    case CEC_LOG_NOTICE:
      strLevel = "NOTICE:  ";
      break;
    case CEC_LOG_TRAFFIC:
      strLevel = "TRAFFIC: ";
      break;
    case CEC_LOG_DEBUG:
      strLevel = "DEBUG:   ";
      break;
    default:
      break;
    }

    printf("%s\t%s\n", strLevel, message->message);
  }
}


static int cec_process_command_as()
{
      int isactive = 0;
      int timeout = 15;
    g_iface.set_active_source(g_iface.connection, g_config.deviceTypes.types[0]);
    // wait for the source switch to finish for 15 seconds tops
 
    while (timeout-- > 0)
    {
    isactive = g_iface.is_libcec_active_source(g_iface.connection);
    if (!isactive)
        sleep(1);
    }
    
    return 1;
  
}



int cec_init()
{
  char buffer[100];
  
  libcecc_reset_configuration(&g_config);
  g_config.clientVersion        = LIBCEC_VERSION_CURRENT;
  g_config.bActivateSource      = 0;
  g_config.callbacks            = &g_callbacks;
  snprintf(g_config.strDeviceName, sizeof(g_config.strDeviceName), "VDR CEC");

  
  if (g_cecLogLevel == -1)
    g_cecLogLevel = g_cecDefaultLogLevel;

  
   g_config.deviceTypes.types[0] = CEC_DEVICE_TYPE_RECORDING_DEVICE;
  

  if (libcecc_initialise(&g_config, &g_iface, NULL) != 1)
  {
    //printf("can't initialise libCEC\n");
    return 0;
  }

  // init video on targets that need this
  //g_iface.init_video_standalone(g_iface.connection);

  
  //g_iface.version_to_string(g_config.serverVersion, buffer, sizeof(buffer));
  //printf("CEC Parser created - libCEC version %s\n", buffer);


  if (g_strPort[0] == 0)
  {
    cec_adapter devices[10];
    int8_t iDevicesFound;

    iDevicesFound = g_iface.find_adapters(g_iface.connection, devices, sizeof(devices) / sizeof(devices), NULL);
    if (iDevicesFound <= 0)
    {
      
      printf("CEC autodetect ");
      printf("FAILED\n");
      libcecc_destroy(&g_iface);
      return 0;
    }
    else
    {
      //printf("\n path:     %s\n com port: %s\n\n", devices[0].path, devices[0].comm);
      strcpy(g_strPort, devices[0].comm);
    }
  }

  //printf("opening a connection to the CEC adapter...\n");

  if (!g_iface.open(g_iface.connection, g_strPort, 5000))
  {
    //printf("unable to open the device on port %s\n", g_strPort);
    libcecc_destroy(&g_iface);
    return 0;
  }

  return 1;
}


int cec_send_command(int dev,char *buffer) {
    
  cec_command tx;

  tx.initiator = 1;
  tx.destination = dev;
  tx.ack = 0; 
  tx.eom = 0;
  tx.opcode = CEC_OPCODE_USER_CONTROL_PRESSED;
  tx.parameters.size = 1;
  tx.opcode_set = 1;
  tx.transmit_timeout = 1000;

  if (strcmp("up",buffer) == 0) {
      tx.parameters.data[0]= CEC_USER_CONTROL_CODE_VOLUME_UP;
  } else {
      tx.parameters.data[0]= CEC_USER_CONTROL_CODE_VOLUME_DOWN;
  }

  libcec_transmit(g_iface.connection,&tx);
  return 0;

#if 0
    //cec_process_command_as();
    if (strcmp("up",buffer) == 0) {
        libcec_volume_up(g_iface.connection,0);
    } else {
        libcec_volume_down(g_iface.connection,0);
    }
    return 0;
#endif
}

int cec_exit() {
    libcecc_destroy(&g_iface);
    return 0;
}
