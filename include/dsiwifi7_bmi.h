// SPDX-License-Identifier: MIT
//
// Copyright (C) 2026 PoroCYon

/// @file dsiwifi7_bmi.h
///
/// @brief BMI utilities for DSi Wifi
///
/// This header defines EXPERIMENTAL functions for low-level access to the
/// WiFi card hardware, including functions for uploading code to the Xtensa
/// CPU in the WiFi card.
///
/// Please do not assume this API is stable, it might still change in the-
/// future.


#ifndef DSIWIFI7_BMI_H_
#define DSIWIFI7_BMI_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <nds/ndstypes.h>

#include <dswifi_version.h>

//        --- SDIO function spaces ---

/// Read a byte from the SDIO Function 0 address space
///
/// @param addr
///     Function 0 address to read from
///
/// @return
///     The byte value at the Function 0 address
u8 SDIO_func0_read_u8(u32 addr);
/// Write a byte to the SDIO Function 0 address space
///
/// @param addr
///     Function 0 address to write to
/// @param v
///     value to write
/// @return
///     nonzero on error
int SDIO_func0_write_u8(u32 addr, u8 val);

/// Read a byte from the SDIO Function 1 address space
///
/// @param addr
///     Function 1 address to read from
///
/// @return
///     The byte value at the Function 1 address
u8 SDIO_func1_read_u8(u32 addr);
/// Write a byte to the SDIO Function 1 address space
///
/// @param addr
///     Function 1 address to write to
/// @param v
///     value to write
/// @return
///     nonzero on error
int SDIO_func1_write_u8(u32 addr, u8 v);
/// Read a 32-bit word from the SDIO Function 1 address space
///
/// @param addr
///     Function 1 address to read from
///
/// @return
///     The word at the Function 1 address
u32 SDIO_func1_read_u32(u32 addr);
/// Write a 32-bit word to the SDIO Function 1 address space
///
/// @param addr
///     Function 1 address to write to
/// @param v
///     value to write
void SDIO_func1_write_u32(u32 addr, u32 val);

//        --- internal MBOX FIFO stuff ---

/// Write a 32-bit word to the MBOX0 FIFO
///
/// @param val
///     value to write
/// @param send_irq
///     whether to interrupt the Xtensa core of the Wifi module
void ath6k_mbox0_write_u32(u32 val, bool send_irq);
/// Read a 32-bit word from the MBOX0 FIFO, with timeout
///
/// @param retv
///     pointer to write the return value to. only written to in case no timeout
///     occurs.
/// @param timeout_max
///     maximum tries before a timeout is signalled
/// @return
///     true if the read succeeded, false if a timeout occurred
bool ath6k_mbox0_read_u32_timeout(u32* retv, u32 timeout_max);
/// Read a block of data from the MBOX0 FIFO
///
/// @param dat
///     buffer to read into
/// @param sz_buf
///     maximum buffer size
/// @param len_read
///     number of bytes to read, must not exceed 0x1800!
/// @return Number of bytes read
u16 ath6k_mbox0_readbytes(u8 *dat, u32 sz_buf, u32 len_read);
/// Send a block of data to the MBOX0 FIFO
///
/// @param dat
///     data to send
/// @param len
///     number of bytes to send, must not exceed 0x1800!
void ath6k_mbox0_sendbytes(const u8 *dat, u32 len);

/// Read a 32-bit word from the Xtensa internal address space
///
/// @param addr
///     address to read from
/// @return
///     word read
u32 ath6k_read_intern_word(u32 addr);
/// Writes a 32-bit word into the Xtensa internal address space
///
/// @param addr
///     address to write to
/// @param data
///     value to write
void ath6k_write_intern_word(u32 addr, u32 data);

//        --- BMI commands ---

/// Perform a busywait until it is possible to send more data via BMI
void ath6k_bmi_wait_count4(void);
/// Set the runtime Xtensa firmware entrypoint
///
/// @param addr
///     firmware entrypoint address
void ath6k_bmi_set_entrypoint(u32 addr);
/// Start the firmware at the address specified by wifi_card_bmi_set_entrypoint()
void ath6k_bmi_start_firmware(void);
/// Call an Xtensa subroutine from BMI mode
///
/// @param addr
///     Function address to call
/// @param arg
///     Argument to pass to thre function
/// @return
///     Function call return value
u32 ath6k_bmi_execute(u32 addr, u32 arg);
/// Reads an Xtensa-internal I/O register
///
/// @param addr
///     I/O register address
/// @return
///     I/O register value
u32 ath6k_bmi_read_register(u32 addr);
/// Writes an Xtensa-internal I/O register
///
/// @param addr
///     I/O register address
/// @param val
///     I/O register value
void ath6k_bmi_write_register(u32 addr, u32 val);
/// Reads the wifi module version
///
/// @return
///     module version
///
///     DWM-W015 = 0x20000188
///     DWM-W024 = 0x23000024
///     DWM-W028 = 0x2300006F
u32 ath6k_bmi_get_version(void);
/// Read a block of Xtensa-internal memory via BMI
///
/// @param addr
///     address to read from
/// @param len
///     number of bytes to read
/// @param data
///     buffer to return read data into
void ath6k_bmi_read_memory(u32 addr, u32 len, u8* data);
/// Writes a block of data into Xtensa-internal memory via BMI
///
/// @param addr
///     address to write to
/// @param data
///     data to write
/// @param len
///     number of bytes to write
void ath6k_bmi_write_memory(u32 addr, const u8* data, u32 len);
/// Upload LZ-compressed data to the Xtensa-internal address space. The BMI
/// bootloader decompresses it.
///
/// @param addr
///     Xtensa-internal address to decompress the data to
/// @param data
///     LZ-compressed data to transfer and decompress
/// @param len
///     size of compressed data
void ath6k_bmi_lz_upload(u32 addr, const u8* data, u32 len);


//        --- general control ---

/// Initializes the Wifi SDIO card into BMI (bootloader) mode
///
/// @return
///     nonzero on error
int ath6k_init_hw_to_bmi(void);
/// Deinitializes the Wifi SDIO card
void ath6k_deinit_hw(void);

/// Sets whether the SDIO card interrupt is enabled
///
/// @note
///     You need to do more work than just calling this function to use IRQs
///     from the Xtensa MBOX SDIO interface. Initialize things as follows:
///
///         irqSetAUX(IRQ_WIFI_SDIO_CARDIRQ, YOUR_ISR_HERE);
///         irqEnableAUX(IRQ_WIFI_SDIO_CARDIRQ);
///         SDIO_enable_cardirq(true);
///         SDIO_func1_write_u32(0x418, 0x010300D1);
///         SDIO_func0_write_u8(0x4, 0x3); // CCCR irq_enable, main+func1
///
/// @param en
///     True for enable the IRQ, false for disable
void SDIO_enable_cardirq(bool en);
/// Gets whether the SDIO card interrupt is pending
///
/// @return
///     SDIO card IRQ status bits
u16 SDIO_get_cardirq_stat(void);

#ifdef __cplusplus
}
#endif

#endif /* DSIWIFI7_BMI_H_ */

