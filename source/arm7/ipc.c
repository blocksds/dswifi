// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#include <dswifi7.h>

#include "arm7/ipc.h"
#include "arm7/setup.h"
#include "arm7/update.h"

volatile Wifi_MainStruct *WifiData = NULL;

static void wifiAddressHandler(void *address, void *userdata)
{
    (void)userdata;

    Wifi_Init(address);
}

static void wifiValue32Handler(u32 value, void *data)
{
    (void)data;

    switch (value)
    {
        case WIFI_SYNC:
            Wifi_Update();
            break;

        case WIFI_DEINIT:
            Wifi_Deinit();
            break;

        default:
            break;
    }
}

void Wifi_CallSyncHandler(void)
{
    fifoSendValue32(FIFO_DSWIFI, WIFI_SYNC);
}

void installWifiFIFO(void)
{
    fifoSetValue32Handler(FIFO_DSWIFI, wifiValue32Handler, 0);
    fifoSetAddressHandler(FIFO_DSWIFI, wifiAddressHandler, 0);
}

int Wifi_RxBufferAllocBuffer(size_t total_size)
{
    u8 *rxbufData = (u8 *)WifiData->rxbufData;

    u32 write_idx = WifiData->rxbufWrite;
    u32 read_idx = WifiData->rxbufRead;

    assert((read_idx & 3) == 0); // Packets must be aligned to 32 bit
    assert((write_idx & 3) == 0);

    if (read_idx <= write_idx)
    {
        if ((write_idx + total_size) >= WIFI_RXBUFFER_SIZE)
        {
            // The packet doesn't fit at the end of the buffer:
            //
            //                    | NEW |
            //
            // | ......... | XXXX | . |           ("X" = Used, "." = Empty)
            //            RD      WR

            // Try to fit it at the beginning. Don't wrap it.
            if (total_size >= read_idx)
            {
                // The packet doesn't fit anywhere:

                // | NEW |            | NEW |
                //
                // | . | XXXXXXXXXXXX | . |
                //     RD             WR

                return -1;
            }

            write_u32(rxbufData + write_idx, WIFI_SIZE_WRAP);
            write_idx = 0;
        }
        else
        {
            // The packet fits at the end:
            //
            //               | NEW |
            //
            // | .... | XXXX | ...... |
            //       RD      WR
        }
    }
    else
    {
        if ((write_idx + total_size) >= read_idx)
        {
            //      | NEW |
            //
            // | XX | . | XXXXXXXXXXX |
            //     WR   RD

            return -1;
        }

        //      | NEW |
        //
        // | XX | ........ | XXXX |
        //     WR          RD
    }

    return write_idx;
}
