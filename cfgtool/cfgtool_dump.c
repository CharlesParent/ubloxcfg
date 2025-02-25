/* ************************************************************************************************/ // clang-format off
// u-blox 9 positioning receivers configuration tool
//
// Copyright (c) 2020 Philippe Kehl (flipflip at oinkzwurgl dot org),
// https://oinkzwurgl.org/hacking/ubloxcfg
//
// This program is free software: you can redistribute it and/or modify it under the terms of the
// GNU General Public License as published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
// even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with this program.
// If not, see <https://www.gnu.org/licenses/>.

#include <string.h>
#include <stddef.h>
#include <signal.h>

#include "cfgtool_util.h"

#include "ff_rx.h"
#include "ff_ubx.h"
#include "ff_epoch.h"

#include "cfgtool_dump.h"

/* ****************************************************************************************************************** */

const char *dumpHelp(void)
{
    return
// -----------------------------------------------------------------------------
"Command 'dump':\n"
"\n"
"    Usage: cfgtool dump -p <port> [-o <outfile>] [-y] [-x] [-n]\n"
"\n"
"    Connects to the receiver and outputs information on the received messages\n"
"    and optionally a hex dump of the messages until SIGINT (e.g. CTRL-C)"NOT_WIN(", SIGHUP")"\n"
"    or SIGTERM is received.\n"
"\n"
"    Returns success (0) if receiver was detected and at least one message was\n"
"    received. Otherwise returns "STRINGIFY(EXIT_RXFAIL)" (rx not detected) or "STRINGIFY(EXIT_RXNODATA)" (no messages).\n"
"\n"
"    Examples:\n"
"\n"
#ifdef _WIN32
"        cfgtool dump -p COM3\n"
#else
"        cfgtool dump -p /dev/ttyUSB0\n"
"        timeout 20 cfgtool dump -p /dev/ttyACM0\n"
#endif
"\n";
}

/* ****************************************************************************************************************** */

bool gAbort;

static void _sigHandler(int signal)
{
    if ( (signal == SIGINT) || (signal == SIGTERM) NOT_WIN(|| (signal == SIGHUP)) )
    {
        PRINT("Aborting...");
        gAbort = true;
    }
}

int dumpRun(const char *portArg, const bool extraInfo, const bool noProbe)
{
    RX_ARGS_t args = RX_ARGS_DEFAULT();
    if (noProbe)
    {
        args.autobaud = false;
        args.detect   = false;
    }
    RX_t *rx = rxInit(portArg, &args);
    if ( (rx == NULL) || !rxOpen(rx) )
    {
        free(rx);
        return EXIT_RXFAIL;
    }

    uint32_t nNmea = 0, sNmea = 0;
    uint32_t nUbx  = 0, sUbx  = 0;
    uint32_t nRtcm = 0, sRtcm = 0;
    uint32_t nNova = 0, sNova = 0;
    uint32_t nGarb = 0, sGarb = 0;
    uint32_t nMsgs = 0, sMsgs = 0;

    gAbort = false;
    signal(SIGINT, _sigHandler);
    signal(SIGTERM, _sigHandler);
    NOT_WIN( signal(SIGHUP, _sigHandler) );

    const uint32_t tOffs = TIME() - timeOfDay(); // Offset between wall clock and parser time reference

    EPOCH_t coll;
    EPOCH_t epoch;
    epochInit(&coll);

    PRINT("Dumping received data...");
    while (!gAbort)
    {
        PARSER_MSG_t *msg = rxGetNextMessage(rx);
        if (msg != NULL)
        {
            const uint32_t latency = (msg->ts - tOffs) % 1000; // Relative to wall clock top of second
            nMsgs++;
            sMsgs += msg->size;
            if (epochCollect(&coll, msg, &epoch))
            {
                ioOutputStr("epoch %4u, %s\n", epoch.seq, epoch.str);
                if (!ioWriteOutput(nMsgs == 1 ? false : true))
                {
                    break;
                }
            }
            const char *prot = "?";
            switch (msg->type)
            {
                case PARSER_MSGTYPE_UBX:
                    prot = "UBX";
                    nUbx++;
                    sUbx += msg->size;
                    break;
                case PARSER_MSGTYPE_NMEA:
                    prot = "NMEA";
                    nNmea++;
                    sNmea += msg->size;
                    break;
                case PARSER_MSGTYPE_RTCM3:
                    prot = "RTMC3";
                    nRtcm++;
                    sRtcm += msg->size;
                    break;
                case PARSER_MSGTYPE_NOVATEL:
                    prot = "NOVATEL";
                    nNova++;
                    sNova += msg->size;
                    break;
                case PARSER_MSGTYPE_GARBAGE:
                    prot = "GARBAGE";
                    nGarb++;
                    sGarb += msg->size;
                    break;
            }
            ioOutputStr("message %4u, dt %4u, size %4d, %-8s %-20s %s\n",
                msg->seq, latency, msg->size, prot, msg->name, msg->info != NULL ? msg->info : "n/a");
            if (extraInfo)
            {
                ioAddOutputHexdump(msg->data, msg->size);
            }
            if (!ioWriteOutput(nMsgs == 1 ? false : true))
            {
                break;
            }
        }
        // No data, yield
        else
        {
            SLEEP(5);
        }
    }

    ioOutputStr("stats UBX      count %6u (%5.1f%%)  size %10u (%5.1f%%)\n", nUbx,  nMsgs > 0 ? (double)nUbx  / (double)nMsgs * 1e2 : 0.0, sUbx,  sMsgs > 0 ? (double)sUbx  / (double)sMsgs * 1e2 : 0.0);
    ioOutputStr("stats NMEA     count %6u (%5.1f%%)  size %10u (%5.1f%%)\n", nNmea, nMsgs > 0 ? (double)nNmea / (double)nMsgs * 1e2 : 0.0, sNmea, sMsgs > 0 ? (double)sNmea / (double)sMsgs * 1e2 : 0.0);
    ioOutputStr("stats RTCM3    count %6u (%5.1f%%)  size %10u (%5.1f%%)\n", nRtcm, nMsgs > 0 ? (double)nRtcm / (double)nMsgs * 1e2 : 0.0, sRtcm, sMsgs > 0 ? (double)sRtcm / (double)sMsgs * 1e2 : 0.0);
    ioOutputStr("stats NOVATEL  count %6u (%5.1f%%)  size %10u (%5.1f%%)\n", nUbx,  nMsgs > 0 ? (double)nNova / (double)nMsgs * 1e2 : 0.0, sNova, sMsgs > 0 ? (double)sNova / (double)sMsgs * 1e2 : 0.0);
    ioOutputStr("stats GARBAGE  count %6u (%5.1f%%)  size %10u (%5.1f%%)\n", nGarb, nMsgs > 0 ? (double)nGarb / (double)nMsgs * 1e2 : 0.0, sGarb, sMsgs > 0 ? (double)sGarb / (double)sMsgs * 1e2 : 0.0);
    ioOutputStr("stats Total    count %6u (100.0%%)  size %10u (100.0%%)\n", nMsgs, sMsgs);
    bool res = ioWriteOutput(true);

    rxClose(rx);
    free(rx);

    return res ? (nMsgs > 0 ? EXIT_SUCCESS : EXIT_RXNODATA) : EXIT_OTHERFAIL;
}

/* ****************************************************************************************************************** */
// eof
