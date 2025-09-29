// SPDX-License-Identifier: MIT
//
// Copyright (C) 2021 Max Thomas

#include "arm7/twl/ndma.h"
#include "arm7/twl/sdio.h"

#define WIFI_SDIO_NDMA

static inline u16 wifi_sdio_read16(u32 reg)
{
    return *(vu16*)(REG_SDIO_BASE + reg);
}

static inline u32 wifi_sdio_read32(u32 reg)
{
    return *(vu32*)(REG_SDIO_BASE + reg);
}

static inline void wifi_sdio_write16(u32 reg, u16 val)
{
    *(vu16*)(REG_SDIO_BASE + reg) = val;
}

static inline void wifi_sdio_write32(u32 reg, u32 val)
{
    *(vu32*)(REG_SDIO_BASE + reg) = val;
}

static inline void wifi_sdio_mask16(u32 reg, u16 clear, u16 set)
{
    u16 val = wifi_sdio_read16(reg);
    val &= ~clear;
    val |= set;
    wifi_sdio_write16(reg, val);
}

static inline void wifi_sdio_mask32(u32 reg, u32 clear, u32 set)
{
    u32 val = wifi_sdio_read32(reg);
    val &= ~clear;
    val |= set;
    wifi_sdio_write32(reg, val);
}

void wifi_sdio_controller_init(void)
{
    wifi_sdio_mask16(WIFI_SDIO_OFFS_IRQ32, 0x0800, 0x0000);
    wifi_sdio_mask16(WIFI_SDIO_OFFS_IRQ32, 0x1000, 0x0000);
    wifi_sdio_mask16(WIFI_SDIO_OFFS_IRQ32, 0x0000, 0x0402);

    wifi_sdio_mask16(WIFI_SDIO_OFFS_DATA16_CNT, 0x0022, 0x0002);

    wifi_sdio_mask16(WIFI_SDIO_OFFS_DATA16_CNT, 0x0020, 0x0000);
    wifi_sdio_write16(WIFI_SDIO_OFFS_DATA32_BLK_LEN, 128);

    wifi_sdio_write16(WIFI_SDIO_OFFS_DATA32_BLK_CNT, 0x0001);

    wifi_sdio_mask16(WIFI_SDIO_OFFS_RESET, 0x0003, 0x0000);
    wifi_sdio_mask16(WIFI_SDIO_OFFS_RESET, 0x0000, 0x0003);

    // Disable all interrupts.
    wifi_sdio_write32(WIFI_SDIO_OFFS_IRQ_MASK, 0xFFFFFFFF);

    wifi_sdio_mask16(0x0FC, 0x0000, 0x00DB);
    wifi_sdio_mask16(0x0FE, 0x0000, 0x00DB);

    wifi_sdio_mask16(WIFI_SDIO_OFFS_PORT_SEL, 0b11, 0);

    wifi_sdio_write16(WIFI_SDIO_OFFS_CLK_CNT, 0x0020);
    wifi_sdio_write16(WIFI_SDIO_OFFS_CARD_OPT, 0x40EE);

    wifi_sdio_mask16(WIFI_SDIO_OFFS_IRQ32, 0x8000, 0x0000);
    wifi_sdio_mask16(WIFI_SDIO_OFFS_IRQ32, 0x0000, 0x0100);
    wifi_sdio_mask16(WIFI_SDIO_OFFS_IRQ32, 0x0100, 0x0000);

    wifi_sdio_mask16(WIFI_SDIO_OFFS_PORT_SEL, 0b11, 0);

    wifi_sdio_write16(WIFI_SDIO_OFFS_DATA16_BLK_LEN, 128);
    wifi_sdio_write16(WIFI_SDIO_OFFS_STOP, 0x0100);
}

void wifi_sdio_send_command(wifi_sdio_ctx *ctx, wifi_sdio_command cmd, u32 args)
{
    if (!ctx)
        return;

    // Safety fallback
    if (!ctx->block_size)
        ctx->block_size = 512;

    void* buffer = ctx->buffer;
    size_t size = ctx->size;

    ctx->status = 0;
    u16 stat0 = 0, stat1 = 0;
    u16 stat0_completion_flags = 0;

    u16 cnt32 = 0;

    // Are we expecting a response? We need to wait for it.
    if (cmd.response_type != wifi_sdio_resp_none)
        stat0_completion_flags |= WIFI_SDIO_STAT0_CMDRESPEND;

    // Are we doing a data transfer? We need to wait for it to end.
    if (cmd.data_transfer)
        stat0_completion_flags |= WIFI_SDIO_STAT0_DATAEND;

#ifdef WIFI_SDIO_DEBUG
    if (ctx->debug)
    {
        WLOG_PRINTF("T: CMD#: 0x%X (%u) (%X) -> %u:%u\n",
                    cmd.raw, cmd.cmd, stat0_completion_flags, ctx->port, ctx->address);
        WLOG_FLUSH();
    }
#endif

    bool buffer32 = false;
    if (buffer && ((u32)buffer & 3) == 0)
        buffer32 = true;

    // Force alignment
    if (buffer && !buffer32)
    {
        ctx->status |= 4;
        return;
    }

    // Wait until the SDIO controller is not busy.
    while (wifi_sdio_read16(WIFI_SDIO_OFFS_IRQ_STAT1) & WIFI_SDIO_STAT1_CMD_BUSY);

    // ACK all interrupts and halt
    wifi_sdio_write16(WIFI_SDIO_OFFS_IRQ_STAT0, 0);
    wifi_sdio_write16(WIFI_SDIO_OFFS_IRQ_STAT1, 0);
    //wifi_sdio_mask16(WIFI_SDIO_OFFS_STOP, 1, 0);

    // Write command arguments.
    wifi_sdio_write16(WIFI_SDIO_OFFS_CMD_PARAM0, args & 0xFFFF);
    wifi_sdio_write16(WIFI_SDIO_OFFS_CMD_PARAM1, args >> 16);

    // Set block len and counts
    wifi_sdio_write16(WIFI_SDIO_OFFS_DATA16_BLK_LEN, ctx->block_size);
    wifi_sdio_write16(WIFI_SDIO_OFFS_DATA16_BLK_CNT, size / ctx->block_size);

    wifi_sdio_write16(WIFI_SDIO_OFFS_DATA32_BLK_LEN, ctx->block_size);
    wifi_sdio_write16(WIFI_SDIO_OFFS_DATA32_BLK_CNT, size / ctx->block_size);

    // Data32 mode
    bool is_block = (cmd.data_length == wifi_sdio_multiple_block);
    if (is_block)
    {
        wifi_sdio_write16(WIFI_SDIO_OFFS_DATA16_CNT, 0x0002);
        if (cmd.data_direction == wifi_sdio_data_read)
            wifi_sdio_write16(WIFI_SDIO_OFFS_IRQ32, 0xC02);
        else
            wifi_sdio_write16(WIFI_SDIO_OFFS_IRQ32, 0x1402);
    }

    // Write command.
    wifi_sdio_write16(WIFI_SDIO_OFFS_CMD, cmd.raw);

#ifdef WIFI_SDIO_NDMA
    if (cmd.data_direction == wifi_sdio_data_read && is_block && buffer)
    {
        wifi_ndma_read(buffer, size);
        return;
    }
    else if (cmd.data_direction == wifi_sdio_data_write && is_block && buffer)
    {
        wifi_ndma_write(buffer, size);
        return;
    }
#endif

    while (true)
    {
        stat1 = wifi_sdio_read16(WIFI_SDIO_OFFS_IRQ_STAT1);

        cnt32 = wifi_sdio_read16(WIFI_SDIO_OFFS_IRQ32);

        // Ready to receive data.
        if (cnt32 & 0x100)
        {
            // Are we actually meant to receive data?
            if (cmd.data_direction == wifi_sdio_data_read && buffer)
            {
                // ACK ready state.
                wifi_sdio_mask16(WIFI_SDIO_OFFS_IRQ_STAT1, WIFI_SDIO_STAT1_RXRDY, 0);

                if (size > (size_t)(ctx->block_size - 1))
                {
//#ifdef WIFI_SDIO_NDMA
                    //wifi_ndma_read(buffer, ctx->block_size);
                    //buffer += ctx->block_size;
//#else
                    void *buffer_end = buffer + ctx->block_size;

                    while (buffer != buffer_end)
                    {
                        *(u32*)buffer = wifi_sdio_read32(WIFI_SDIO_OFFS_DATA32_FIFO);

                        // WLOG_PRINTF("asdf %x\n", *(u32*)buffer);
                        // WLOG_FLUSH();

                        buffer += sizeof(u32);
                    }
//#endif
                    size -= ctx->block_size;
                }
            }
        }

        // Data transmission requested.
        // if (!(cnt32 & 0x200))
        if (stat1 & WIFI_SDIO_STAT1_TXRQ)
        {
            // Are we actually meant to write data?
            if (cmd.data_direction == wifi_sdio_data_write && buffer)
            {
                // ACK request.
                wifi_sdio_mask16(WIFI_SDIO_OFFS_IRQ_STAT1, WIFI_SDIO_STAT1_TXRQ, 0);

                if (size > (size_t)(ctx->block_size - 1))
                {
//#ifdef WIFI_SDIO_NDMA
                    //wifi_ndma_write(buffer, ctx->block_size);
                    //buffer += ctx->block_size;
//#else
                    void *buffer_end = buffer + ctx->block_size;

                    while (buffer != buffer_end)
                    {
                        u32 data = *(u32*)buffer;
                        buffer += sizeof(u32);
                        wifi_sdio_write32(WIFI_SDIO_OFFS_DATA32_FIFO, data);
                    }
//#endif
                    size -= ctx->block_size;
                }
            }
        }

        // Has an error been asserted? Exit if so.
        if (stat1 & WIFI_SDIO_MASK_ERR)
        {
#ifdef WIFI_SDIO_DEBUG
            if (ctx->debug)
            {
                WLOG_PRINTF("T: ERR#: %X 0000\n", stat1 & WIFI_SDIO_MASK_ERR);
                WLOG_FLUSH();
            }
#endif
            // Error flag.
            ctx->status |= 4;
            break;
        }

        bool end_cond = !(stat1 & WIFI_SDIO_STAT1_CMD_BUSY);
        if (is_block)
        {
            stat0 = wifi_sdio_read16(WIFI_SDIO_OFFS_IRQ_STAT0);
            end_cond = (stat0 & WIFI_SDIO_STAT0_CMDRESPEND && !size);
        }

        // Not busy...
        if (end_cond)
        {
            stat0 = wifi_sdio_read16(WIFI_SDIO_OFFS_IRQ_STAT0);

            // Set response end flag.
            if (stat0 & WIFI_SDIO_STAT0_CMDRESPEND)
                ctx->status |= 1;

            // Set data end flag.
            if (stat0 & WIFI_SDIO_STAT0_DATAEND)
                ctx->status |= 2;

            // If done (all completion criteria), exit.
            if ((stat0 & stat0_completion_flags) == stat0_completion_flags)
                break;
        }

        if (ctx->break_early && !size)
            break;
    }

    ctx->stat0 = wifi_sdio_read16(WIFI_SDIO_OFFS_IRQ_STAT0);
    ctx->stat1 = wifi_sdio_read16(WIFI_SDIO_OFFS_IRQ_STAT1);

    ctx->err0 = wifi_sdio_read16(WIFI_SDIO_OFFS_ERR_DETAIL0);
    ctx->err1 = wifi_sdio_read16(WIFI_SDIO_OFFS_ERR_DETAIL1);

    // ACK all interrupts.
    wifi_sdio_write16(WIFI_SDIO_OFFS_IRQ_STAT0, 0);
    wifi_sdio_write16(WIFI_SDIO_OFFS_IRQ_STAT1, 0);

    // If the command has a response, pull it in to sdmmc_ctx.
    if (cmd.response_type != wifi_sdio_resp_none)
    {
        ctx->resp[0] = wifi_sdio_read16(WIFI_SDIO_OFFS_RESP0)
                     | (u32)(wifi_sdio_read16(WIFI_SDIO_OFFS_RESP1) << 16);
        ctx->resp[1] = wifi_sdio_read16(WIFI_SDIO_OFFS_RESP2)
                     | (u32)(wifi_sdio_read16(WIFI_SDIO_OFFS_RESP3) << 16);
        ctx->resp[2] = wifi_sdio_read16(WIFI_SDIO_OFFS_RESP4)
                     | (u32)(wifi_sdio_read16(WIFI_SDIO_OFFS_RESP5) << 16);
        ctx->resp[3] = wifi_sdio_read16(WIFI_SDIO_OFFS_RESP6)
                     | (u32)(wifi_sdio_read16(WIFI_SDIO_OFFS_RESP7) << 16);
    }

#ifdef WIFI_SDIO_DEBUG
    if (ctx->debug)
    {
        WLOG_PRINTF("T: STAT: %X %X (%X) INFO: %X %X\n", ctx->stat1, ctx->stat0,
                    ctx->status, ctx->err1, ctx->err0);

        if (cmd.response_type != wifi_sdio_resp_none)
        {
            if (cmd.response_type == wifi_sdio_resp_136bit)
            {
                WLOG_PRINTF("T: RESP: %X %X %X %X\n",
                            ctx->resp[0], ctx->resp[1], ctx->resp[2], ctx->resp[3]);
            }
            else
            {
                WLOG_PRINTF("T: RESP: %X\n", ctx->resp[0]);
            }
        }

        WLOG_FLUSH();
    }
#endif
}

void wifi_sdio_switch_device(wifi_sdio_ctx *ctx)
{
    if (!ctx)
        return;

    // Reconfigure the bus to talk to the new device.
    wifi_sdio_mask16(WIFI_SDIO_OFFS_PORT_SEL, 0b11, ctx->port);
    wifi_sdio_setclk(ctx->clk_cnt);

    // WIFI_SDIO_CARD_OPT bit 15: bus width (0 = 4-bit, 1 = 1-bit).
    if (ctx->bus_width == 4)
        wifi_sdio_mask16(WIFI_SDIO_OFFS_CARD_OPT, BIT(15), 0);
    else
        wifi_sdio_mask16(WIFI_SDIO_OFFS_CARD_OPT, 0, BIT(15));
}

void wifi_sdio_setclk(u32 data)
{
    wifi_sdio_mask16(WIFI_SDIO_OFFS_CLK_CNT, 0x100, 0);
    wifi_sdio_mask16(WIFI_SDIO_OFFS_CLK_CNT, 0x2FF, data & 0x2FF);
    wifi_sdio_mask16(WIFI_SDIO_OFFS_CLK_CNT, 0x0, 0x100);
}

void wifi_sdio_stop(void)
{
    wifi_sdio_write16(WIFI_SDIO_OFFS_STOP, 0x100);
}

void wifi_sdio_enable_cardirq(bool en)
{
    if (en)
    {
        wifi_sdio_mask16(WIFI_SDIO_OFFS_CARDIRQ_CTL, 0, 0x0001);
        wifi_sdio_mask16(WIFI_SDIO_OFFS_CARDIRQ_MASK, 0x0003, 0);
    }
    else
    {
        wifi_sdio_mask16(WIFI_SDIO_OFFS_CARDIRQ_CTL, 1, 0);
        wifi_sdio_mask16(WIFI_SDIO_OFFS_CARDIRQ_MASK, 0, 3);
    }
}

u16 wifi_sdio_get_cardirq_stat(void)
{
    return wifi_sdio_read16(WIFI_SDIO_OFFS_CARDIRQ_STAT);
}
