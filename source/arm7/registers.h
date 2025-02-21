// SPDX-License-Identifier: MIT
//
// Copyright (C) 2005-2006 Stephen Stair - sgstair@akkit.org - http://www.akkit.org
// Copyright (C) 2025 Antonio Niño Díaz

#ifndef DSWIFI_ARM7_REGISTERS_H__
#define DSWIFI_ARM7_REGISTERS_H__

#include <nds.h>

// DS Registers
// ============

#define REG_WIFIWAITCNT         (*((vu16 *)(0x04000206)))

#define WIFI_RAM_N_10_CYCLES    (0)
#define WIFI_RAM_N_8_CYCLES     (1)
#define WIFI_RAM_N_6_CYCLES     (2)
#define WIFI_RAM_N_18_CYCLES    (3)
#define WIFI_RAM_N_MASK         (3)
#define WIFI_RAM_S_6_CYCLES     (0)
#define WIFI_RAM_S_4_CYCLES     (1 << 2)
#define WIFI_RAM_S_MASK         (1 << 2)
#define WIFI_IO_N_10_CYCLES     (0)
#define WIFI_IO_N_8_CYCLES      (1 << 3)
#define WIFI_IO_N_6_CYCLES      (2 << 3)
#define WIFI_IO_N_18_CYCLES     (3 << 3)
#define WIFI_IO_N_MASK          (3 << 3)
#define WIFI_IO_S_6_CYCLES      (0)
#define WIFI_IO_S_4_CYCLES      (1 << 5)
#define WIFI_IO_S_MASK          (1 << 5)

// Wifi registers
// ==============

#define WIFI_REG(ofs)       (*((vu16 *)(0x04800000 + (ofs))))
#define WIFI_REG_ARR(ofs)   (((vu16 *)(0x04800000 + (ofs))))

// Control registers
// -----------------

#define W_ID                WIFI_REG(0x8000)

#define ID_NDS_ORIGINAL     0x1440
#define ID_NDS_LITE         0xC340

#define W_MODE_RST          WIFI_REG(0x8004)

#define W_MODE_WEP          WIFI_REG(0x8006)

#define MODE_WEP_SLEEP_AUTO     (0) // Auto sleep/wakeup mode
#define MODE_WEP_SLEEP_WAKE     (1)
#define MODE_WEP_SLEEP_OFF      (2)
#define MODE_WEP_SLEEP_MANUAL   (3) // Use W_POWERSTATE bit 0..1 manaully
#define MODE_WEP_SLEEP_MASK     (7)

#define MODE_WEP_KEYLEN_64BIT   (1 << 3)
#define MODE_WEP_KEYLEN_128BIT  (2 << 3)
#define MODE_WEP_KEYLEN_152BIT  (3 << 3)
#define MODE_WEP_KEYLEN_SHIFT   (3)
#define MODE_WEP_KEYLEN_MASK    (7 << 3)

#define W_TXSTATCNT         WIFI_REG(0x8008)

#define TXSTATCNT_IRQ_MP_REPLY  BIT(12) ///< Trigger IRQ on REPLY transmit
#define TXSTATCNT_IRQ_MP_ACK    BIT(13) ///< Trigger IRQ on CMD ACK transmit
#define TXSTATCNT_IRQ_MP_CMD    BIT(14) ///< Trigger IRQ on CMD DATA transmit
#define TXSTATCNT_IRQ_BEACON    BIT(15) ///< Trigger IRQ on BEACON transmit

#define W_X_00A             WIFI_REG(0x800A)

#define W_IF                WIFI_REG(0x8010)
#define W_IE                WIFI_REG(0x8012)

#define IRQ_RX_COMPLETE             BIT(0)
#define IRQ_TX_COMPLETE             BIT(1)
#define IRQ_RX_EVENT_INCREMENT      BIT(2)
#define IRQ_TX_EVENT_INCREMENT      BIT(3)
#define IRQ_RX_EVENT_HALF_OVERFLOW  BIT(4)
#define IRQ_TX_ERROR_HALF_OVERFLOW  BIT(5)
#define IRQ_RX_START                BIT(6)
#define IRQ_TX_START                BIT(7)
#define IRQ_TXBUF_COUNT_END         BIT(8)
#define IRQ_RXBUF_COUNT_END         BIT(9)
#define IRQ_UNUSED                  BIT(10)
#define IRQ_RF_WAKEUP               BIT(11)
#define IRQ_MULTIPLAY_CMD_DONE      BIT(12)
#define IRQ_POST_BEACON_TIMESLOT    BIT(13)
#define IRQ_BEACON_TIMESLOT         BIT(14)
#define IRQ_PRE_BEACON_TIMESLOT     BIT(15)
#define IRQ_ALL_BITS                0xFFFF

#define W_MACADDR           WIFI_REG_ARR(0x8018) // 3 registers
#define W_BSSID             WIFI_REG_ARR(0x8020) // 3 registers
#define W_AID_LOW           WIFI_REG(0x8028)
#define W_AID_FULL          WIFI_REG(0x802A)
#define W_TX_RETRYLIMIT     WIFI_REG(0x802C)
// WIFI_REG(0x802E)

#define W_RXCNT             WIFI_REG(0x8030)

#define RXCNT_EMPTY_RXBUF   BIT(0) // W_RXBUF_WR_ADDR -> W_RXBUF_WRCSR
#define RXCNT_MP_SWAP_TXBUF BIT(7) // W_TXBUF_REPLY1 -> W_TXBUF_REPLY2; 0 -> W_TXBUF_REPLY1
#define RXCNT_ENABLE_RX     BIT(15)

#define W_WEP_CNT           WIFI_REG(0x8032)

#define WEP_CNT_ENABLE      BIT(15)
#define WEP_CNT_DISABLE     0

// WIFI_REG(0x8034)

// Powerdown registers
// -------------------

#define W_POWER_US          WIFI_REG(0x8036)
#define W_POWER_TX          WIFI_REG(0x8038)
#define W_POWERSTATE        WIFI_REG(0x803C)
#define W_POWERFORCE        WIFI_REG(0x8040)
#define W_RANDOM            WIFI_REG(0x8044)
#define W_POWER_048         WIFI_REG(0x8048)

// Receive Control/Memory
// ----------------------

#define W_RXBUF_BEGIN       WIFI_REG(0x8050)
#define W_RXBUF_END         WIFI_REG(0x8052)
#define W_RXBUF_WRCSR       WIFI_REG(0x8054)
#define W_RXBUF_WR_ADDR     WIFI_REG(0x8056)
#define W_RXBUF_RD_ADDR     WIFI_REG(0x8058)
#define W_RXBUF_READCSR     WIFI_REG(0x805A)
#define W_RXBUF_COUNT       WIFI_REG(0x805C)
#define W_RXBUF_RD_DATA     WIFI_REG(0x8060)
#define W_RXBUF_GAP         WIFI_REG(0x8062)
#define W_RXBUF_GAPDISP     WIFI_REG(0x8064)

// Transmit Control/Memory
// -----------------------

#define W_TXBUF_WR_ADDR     WIFI_REG(0x8068)
#define W_TXBUF_COUNT       WIFI_REG(0x806C)
#define W_TXBUF_WR_DATA     WIFI_REG(0x8070)
#define W_TXBUF_GAP         WIFI_REG(0x8074)
#define W_TXBUF_GAPDISP     WIFI_REG(0x8076)
// WIFI_REG(0x8078)

#define W_TXBUF_BEACON      WIFI_REG(0x8080)

#define TXBUF_BEACON_ENABLE     BIT(15)
#define TXBUF_BEACON_DISABLE    0

#define W_TXBUF_TIM         WIFI_REG(0x8084)
#define W_LISTENCOUNT       WIFI_REG(0x8088)
#define W_BEACONINT         WIFI_REG(0x808C)
#define W_LISTENINT         WIFI_REG(0x808E)

#define W_TXBUF_CMD         WIFI_REG(0x8090)

#define TXBUF_CMD_ENABLE    BIT(15)

#define W_TXBUF_REPLY1      WIFI_REG(0x8094)
#define W_TXBUF_REPLY2      WIFI_REG(0x8098)

#define TXBUF_REPLY_ENABLE  BIT(15)

// WIFI_REG(0x809C)

#define W_TXBUF_LOC1        WIFI_REG(0x80A0)
#define W_TXBUF_LOC2        WIFI_REG(0x80A4)
#define W_TXBUF_LOC3        WIFI_REG(0x80A8)

#define TXBUF_LOCN_ENABLE   BIT(15)

#define W_TXREQ_RESET       WIFI_REG(0x80AC)
#define W_TXREQ_SET         WIFI_REG(0x80AE)
#define W_TXREQ_READ        WIFI_REG(0x80B0)
#define W_TXBUF_RESET       WIFI_REG(0x80B4)
#define W_TXBUSY            WIFI_REG(0x80B6)

#define TXBIT_LOC1          BIT(0)
#define TXBIT_CMD           BIT(1)
#define TXBIT_LOC2          BIT(2)
#define TXBIT_LOC3          BIT(3)
#define TXBIT_BEACON        BIT(4)
#define TXBIT_ALL           0xFFFF

#define W_TXSTAT            WIFI_REG(0x80B8)
// WIFI_REG(0x80BA)
#define W_PREAMBLE          WIFI_REG(0x80BC)
#define W_CMD_TOTALTIME     WIFI_REG(0x80C0)
#define W_CMD_REPLYTIME     WIFI_REG(0x80C4)
// WIFI_REG(0x80C8)

#define W_RXFILTER          WIFI_REG(0x80D0)

// Set bits to receive that type of frames
#define RXFILTER_MGMT_BEACON_OTHER_BSSID       BIT(0) ///< Beacons with mismatched BSSID, data frame retransmits
#define RXFILTER_DATA_SUBTYPE_1                BIT(1) ///< Data frames, subtype 1 (non-multiplay)
#define RXFILTER_DATA_SUBTYPE_2                BIT(2) ///< Data frames, subtype 2 (non-multiplay)
#define RXFILTER_DATA_SUBTYPE_3                BIT(3) ///< Data frames, subtype 3
#define RXFILTER_DATA_SUBTYPE_5                BIT(4) ///< Data frames, subtype 5
#define RXFILTER_DATA_SUBTYPE_6                BIT(5) ///< Data frames, subtype 6
#define RXFILTER_DATA_SUBTYPE_7                BIT(6) ///< Data frames, subtype 7
#define RXFILTER_MP_ACK                        BIT(7) ///< Multiplay ACKs
#define RXFILTER_MP_EMPTY_REPLY                BIT(8) ///< Empty multiplay replies
#define RXFILTER_MGMT_NONBEACON_OTHER_BSSID    BIT(9) ///< Non-beacon management packets with mismatched BSSID
#define RXFILTER_MGMT_NONBEACON_OTHER_BSSID_EX BIT(10) ///< Same as bit 9
#define RXFILTER_CONTROL_DATA_OTHER_BSSID      BIT(11) ///< Control/data frames with mismatched BSSID
#define RXFILTER_UPDATE_WRCSR                  BIT(12) ///< Update W_RXBUF_WRCSR after IEEE header
#define RXFILTER_ALL                           (0x1FFF) // Bits 13-15 don't exist

#define W_CONFIG_0D4        WIFI_REG(0x80D4)
#define W_CONFIG_0D8        WIFI_REG(0x80D8)
#define W_RX_LEN_CROP       WIFI_REG(0x80DA)

#define W_RXFILTER2         WIFI_REG(0x80E0)

#define RXFILTER2_IGNORE_STA_STA    (1 << 0) ///< Ignore fromSTA/toSTA packets
#define RXFILTER2_IGNORE_STA_DS     (1 << 1) ///< Ignore fromSTA/toDS packets
#define RXFILTER2_IGNORE_DS_STA     (1 << 2) ///< Ignore fromDS/toSTA packets
#define RXFILTER2_IGNORE_DS_DS      (1 << 3) ///< Ignore fromDS/toDS packets

// Wifi Timers
// -----------

#define W_US_COUNTCNT       WIFI_REG(0x80E8)
#define W_US_COMPARECNT     WIFI_REG(0x80EA)
#define W_CONFIG_0EC        WIFI_REG(0x80EC)
#define W_CMD_COUNTCNT      WIFI_REG(0x80EE)
#define W_US_COMPARE0       WIFI_REG(0x80F0)
#define W_US_COMPARE1       WIFI_REG(0x80F2)
#define W_US_COMPARE2       WIFI_REG(0x80F4)
#define W_US_COMPARE3       WIFI_REG(0x80F6)
#define W_US_COUNT0         WIFI_REG(0x80F8)
#define W_US_COUNT1         WIFI_REG(0x80FA)
#define W_US_COUNT2         WIFI_REG(0x80FC)
#define W_US_COUNT3         WIFI_REG(0x80FE)
// WIFI_REG(0x8100)
// WIFI_REG(0x8102)
// WIFI_REG(0x8104)
// WIFI_REG(0x8106)
#define W_CONTENTFREE       WIFI_REG(0x810C)
#define W_PRE_BEACON        WIFI_REG(0x8110)
#define W_CMD_COUNT         WIFI_REG(0x8118)
#define W_BEACON_COUNT      WIFI_REG(0x811C)

// Configuration Ports
// -------------------

#define W_CONFIG_120       WIFI_REG(0x8120)
#define W_CONFIG_122       WIFI_REG(0x8122)
#define W_CONFIG_124       WIFI_REG(0x8124)
// WIFI_REG(0x8126)
#define W_CONFIG_128       WIFI_REG(0x8128)
// WIFI_REG(0x812A)
#define W_CONFIG_130       WIFI_REG(0x8130)
#define W_CONFIG_132       WIFI_REG(0x8132)
#define W_POST_BEACON      WIFI_REG(0x8134)
#define W_CONFIG_140       WIFI_REG(0x8140)
#define W_CONFIG_142       WIFI_REG(0x8142)
#define W_CONFIG_144       WIFI_REG(0x8144)
#define W_CONFIG_146       WIFI_REG(0x8146)
#define W_CONFIG_148       WIFI_REG(0x8148)
#define W_CONFIG_14A       WIFI_REG(0x814A)
#define W_CONFIG_14C       WIFI_REG(0x814C)
#define W_CONFIG_150       WIFI_REG(0x8150)
#define W_CONFIG_154       WIFI_REG(0x8154)

// Baseband Chip Ports
// -------------------

#define W_BB_CNT           WIFI_REG(0x8158)
#define W_BB_WRITE         WIFI_REG(0x815A)
#define W_BB_READ          WIFI_REG(0x815C)
#define W_BB_BUSY          WIFI_REG(0x815E)
#define W_BB_MODE          WIFI_REG(0x8160)
#define W_BB_POWER         WIFI_REG(0x8168)

// Internal
// --------

// WIFI_REG(0x816A)
// WIFI_REG(0x8170)
// WIFI_REG(0x8172)
// WIFI_REG(0x8174)
// WIFI_REG(0x8176)
// WIFI_REG(0x8178)

// RF Chip Ports
// -------------

#define W_RF_DATA2          WIFI_REG(0x817C)
#define W_RF_DATA1          WIFI_REG(0x817E)
#define W_RF_BUSY           WIFI_REG(0x8180)
#define W_RF_CNT            WIFI_REG(0x8184)
// WIFI_REG(0x8190)

#define W_TX_HDR_CNT        WIFI_REG(0x8194)

#define TX_HDR_CNT_POWER_DURATION_MANUAL    BIT(0)
#define TX_HDR_CNT_POWER_DURATION_AUTO      0

#define TX_HDR_CNT_FCS_MANUAL               BIT(1)
#define TX_HDR_CNT_FCS_AUTO                 0

#define TX_HDR_CNT_SEQ_CONTROL_MANUAL       BIT(2)
#define TX_HDR_CNT_SEQ_CONTROL_AUTO         0

// WIFI_REG(0x8198)
#define W_RF_PINS           WIFI_REG(0x819C)
#define W_X_1A0             WIFI_REG(0x81A0)
#define W_X_1A2             WIFI_REG(0x81A2)
#define W_X_1A4             WIFI_REG(0x81A4)

// Wifi Statistics
// ---------------

#define OFF_RXSTAT_INC_IF   0x81A8
#define OFF_RXSTAT_INC_IE   0x81AA
#define OFF_RXSTAT_OVF_IF   0x81AC
#define OFF_RXSTAT_OVF_IE   0x81AE
#define OFF_RXSTAT_1B0      0x81B0
#define OFF_RXSTAT_1B2      0x81B2
#define OFF_RXSTAT_1B4      0x81B4
#define OFF_RXSTAT_1B6      0x81B6
#define OFF_RXSTAT_1B8      0x81B8
#define OFF_RXSTAT_1BA      0x81BA
#define OFF_RXSTAT_1BC      0x81BC
#define OFF_RXSTAT_1BE      0x81BE
#define OFF_TX_ERR_COUNT    0x81C0
#define OFF_RX_COUNT        0x81C4
#define OFF_CMD_STAT_1D0    0x81D0
#define OFF_CMD_STAT_1D2    0x81D2
#define OFF_CMD_STAT_1D4    0x81D4
#define OFF_CMD_STAT_1D6    0x81D6
#define OFF_CMD_STAT_1D8    0x81D8
#define OFF_CMD_STAT_1DA    0x81DA
#define OFF_CMD_STAT_1DC    0x81DC
#define OFF_CMD_STAT_1DE    0x81DE

#define W_RXSTAT_INC_IF     WIFI_REG(OFF_RXSTAT_INC_IF)
#define W_RXSTAT_INC_IE     WIFI_REG(OFF_RXSTAT_INC_IE)
#define W_RXSTAT_OVF_IF     WIFI_REG(OFF_RXSTAT_OVF_IF)
#define W_RXSTAT_OVF_IE     WIFI_REG(OFF_RXSTAT_OVF_IE)
#define W_RXSTAT_1B0        WIFI_REG(OFF_RXSTAT_1B0)
#define W_RXSTAT_1B2        WIFI_REG(OFF_RXSTAT_1B2)
#define W_RXSTAT_1B4        WIFI_REG(OFF_RXSTAT_1B4)
#define W_RXSTAT_1B6        WIFI_REG(OFF_RXSTAT_1B6)
#define W_RXSTAT_1B8        WIFI_REG(OFF_RXSTAT_1B8)
#define W_RXSTAT_1BA        WIFI_REG(OFF_RXSTAT_1BA)
#define W_RXSTAT_1BC        WIFI_REG(OFF_RXSTAT_1BC)
#define W_RXSTAT_1BE        WIFI_REG(OFF_RXSTAT_1BE)
#define W_TX_ERR_COUNT      WIFI_REG(OFF_TX_ERR_COUNT)
#define W_RX_COUNT          WIFI_REG(OFF_RX_COUNT)
#define W_CMD_STAT_1D0      WIFI_REG(OFF_CMD_STAT_1D0)
#define W_CMD_STAT_1D2      WIFI_REG(OFF_CMD_STAT_1D2)
#define W_CMD_STAT_1D4      WIFI_REG(OFF_CMD_STAT_1D4)
#define W_CMD_STAT_1D6      WIFI_REG(OFF_CMD_STAT_1D6)
#define W_CMD_STAT_1D8      WIFI_REG(OFF_CMD_STAT_1D8)
#define W_CMD_STAT_1DA      WIFI_REG(OFF_CMD_STAT_1DA)
#define W_CMD_STAT_1DC      WIFI_REG(OFF_CMD_STAT_1DC)
#define W_CMD_STAT_1DE      WIFI_REG(OFF_CMD_STAT_1DE)

// Internal Diagnostics
// --------------------

// WIFI_REG(0x81F0)
// WIFI_REG(0x8204)
// WIFI_REG(0x8208)
// WIFI_REG(0x820C)
#define W_TX_SEQNO          WIFI_REG(0x8210)
#define W_RF_STATUS         WIFI_REG(0x8214)
#define W_IF_SET            WIFI_REG(0x821C)
#define W_RAM_DISABLE       WIFI_REG(0x8220)
// WIFI_REG(0x8224)
#define W_X_228             WIFI_REG(0x8228)
// WIFI_REG(0x8230)
// WIFI_REG(0x8234)
// WIFI_REG(0x8238)
// WIFI_REG(0x823C)
#define W_X_244             WIFI_REG(0x8244)
// WIFI_REG(0x8248)
// WIFI_REG(0x824C)
// WIFI_REG(0x824E)
// WIFI_REG(0x8250)
#define W_CONFIG_254        WIFI_REG(0x8254)
// WIFI_REG(0x8258)
// WIFI_REG(0x825C)
// WIFI_REG(0x8260)
// WIFI_REG(0x8264)
#define W_RXTX_ADDR         WIFI_REG(0x8268)
// WIFI_REG(0x8270)
// WIFI_REG(0x8274)
// WIFI_REG(0x8278)
// WIFI_REG(0x827C)
#define W_X_290             WIFI_REG(0x8290)
// WIFI_REG(0x8298)
// WIFI_REG(0x82A0)
// WIFI_REG(0x82A2)
// WIFI_REG(0x82A4)
// WIFI_REG(0x82A8)
// WIFI_REG(0x82AC)
// WIFI_REG(0x82B0)
// WIFI_REG(0x82B4)
// WIFI_REG(0x82B8)
// WIFI_REG(0x82C0)
// WIFI_REG(0x82C4)
// WIFI_REG(0x82C8)
// WIFI_REG(0x82CC)
// WIFI_REG(0x82D0)
// WIFI_REG(0x82F0)
// WIFI_REG(0x82F2)
// WIFI_REG(0x82F4)
// WIFI_REG(0x82F6)

// Wifi RAM
// ========

#define W_MACMEM(addr)      WIFI_REG(0x4000 + (addr))
#define W_MACMEM_SIZE       0x2000

// WIFI_REG_ARR(0x5F60)
#define W_WEPKEY_0          WIFI_REG_ARR(0x5F80)
#define W_WEPKEY_1          WIFI_REG_ARR(0x5FA0)
#define W_WEPKEY_2          WIFI_REG_ARR(0x5FC0)
#define W_WEPKEY_3          WIFI_REG_ARR(0x5FE0)

#define WEP_KEY_MAX_SIZE    32

#endif // DSWIFI_ARM7_REGISTERS_H__
