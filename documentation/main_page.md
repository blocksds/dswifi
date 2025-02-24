DSWifi documentation {#mainpage}
====================

## Introduction

A library written in C to use the WiFi hardware of the Nintendo DS. At the
moment it only supports DS mode WiFi (on a DSi it will run in compatibility
mode).

Features:

- It supports accessing the Internet using IPv4.

  - Open networks and WEP-encrypted networks are supported.
  - TCP and UDP supported.

- It supports local multiplayer mode:

  - Up to 15 consoles can connect to the host (the limit can be adjusted).
  - It supports direct host <-> client transfers or optimized group transfers.
  - Client connections to the host are handled by the ARM7 automatically.

## Usage guides

- [Internet mode](internet_mode.md)
- [Local multiplayer mode](local_multiplayer_mode.md)

## Technical documentation

- [Library design](library_design.md)

## API documentation

- ARM9
  - @ref dswifi9_general "General state of the library."
  - @ref dswifi9_ap "Scan and connect to access points."
  - @ref dswifi9_mp_host "Local multiplayer host utilities."
  - @ref dswifi9_ip "Utilities related to Internet access."
  - @ref dswifi9_raw_tx_rx "Raw transfer/reception of packets."

- ARM7
  - @ref dswifi7.h "ARM7 DSWifi header"

- Common
  - @ref dswifi_common.h "ARM9 and ARM7 common definitions"
