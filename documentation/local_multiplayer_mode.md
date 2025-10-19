# Local multiplayer mode guide

DSWifi supports local multiplayer with a limit of up to 15 client consoles
connected to a single host console (the maximum supported by the hardware). The
maximum number of players is decided by the developer of the application. This
document describes how host and client consoles need to use DSWifi to develop
applications that use this kind of multiplayer.

## 1. Information about transfer modes

There are two ways to transfer information between host and clients: multiplayer
transfers and direct communications between the host and a client. Both systems
have advantages and disadvantages:

- Multiplayer transfers:

  - Communications are optimized because the DS WiFi hardware does a lot of the
    work automatically. After the host sends a message and clients receive it,
    they start sending a reply packet automatically without any involvement of
    the CPU. If multiple clients are connected to a host, they send the messages
    in sequence without any kind of intermediate handshakes that would waste
    time. This is achieved by using a Contention Free Period (CFP).

  - You need to define a packet size for host packets and another size for
    client packets. While in theory it is possible to change this dynamically,
    DSWifi expects the developer to define an initial size, and it is only
    possible to send smaller packets later.

  - This mode is generally recommended for games that involve more than one
    client and one host.

- Direct transfers:

  - Communications are done by sending regular data packets, and the application
    needs to organize the response of every packet. This means the
    communications are generally less optimal than with multiplayer transfers.

  - There are no restrictions about the size of the packets (apart from regular
    WiFi limitations). It is possible to send any size of packet at any point.

  - This mode is a good idea if you have a 2-player game in which you don't
    mind losing a bit of latency in exchange for flexibility in the packets you
    send.

## 2. Library initialization

You need to initialize the library skipping the autoconnection step:

```c
Wifi_InitDefault(INIT_ONLY);
```

This is required even if your application needs to switch between Internet and
local multiplayer mode, you can't use `WFC_CONNECT`. Also, you can't use the
DSi driver in multiplayer mode, it will use the DS compatibility mode.

On the ARM7, hardware timer number `LIBNDS_DEFAULT_TIMER_WIFI` will be used by
the WiFi library after this call.

After using multiplayer mode you can call this to switch to Internet mode:

```c
Wifi_InternetMode();
```

If you aren't interested in ever using Internet mode you can initialize the
library like this:

```c
Wifi_InitDefault(INIT_ONLY | WIFI_LOCAL_ONLY);
```

This will prevent the IP library from starting, and it will save over 100 KB of
RAM. It will also prevent `Wifi_InternetMode()` from working until the library
is re-initialized without `WIFI_LOCAL_ONLY`.

If you are done using wireless mode you can disable it to save power by calling:

```c
Wifi_DisableWifi();
```

If you aren't going to use DSWiFi again and you want to free up even more
resources, you can call:

```c
Wifi_Deinit();
```

It will free all RAM used by the library and ARM7 hardware timer number
`LIBNDS_DEFAULT_TIMER_WIFI`. You can call `Wifi_InitDefault()` at a later time
to re-initialize it.

## 3. Host consoles

A host console becomes an access point for other consoles. It is required to
define the size of host packets and client packets for multiplayer transfers
(even if you don't plan to use them). For example:

```c
typedef struct {
    u32 command;
    u32 arg;
} packet_host_to_client;

typedef struct {
    u32 response;
} packet_client_to_host;

// You can leave the packet sizes as 0 if you don't want to use multiplayer
// transfers.
Wifi_MultiplayerHostMode(number_of_players,
                         sizeof(packet_host_to_client),
                         sizeof(packet_client_to_host));

// Wait for the library to enter host mode
while (!Wifi_LibraryModeReady())
    swiWaitForVBlank();
```

Now we can start advertising this access point. For that, you need to setup the
beacon packet information. You need to assign a name to the AP (or pass `NULL`
if you don't care about the name) and a game ID that will be used to identify
APs that have been created by this application. Note that the AP name doesn't
affect anything, and it can safely be left empty. The AP can still be identified
by the game ID:

```c
// Before setting up the beacon, you can also decide which channel to use, and
// whether client connections will be allowed right away or not. For now, a
// good idea is to allow new clients and to pick randomly a channel between 1, 6
// and 11 (Nintendo uses 1, 7 and 13 in official games). Don't change the
// channel after clients have started to connect to the console!
Wifi_SetChannel(7);
Wifi_MultiplayerAllowNewClients(true);

Wifi_BeaconStart("NintendoDS", 0xCAFEF00D);

// You can also call the setup functions after setting up the beacon, but you
// need to wait for at least two or three frames so that the settings are
// applied correctly.
```

At this point client consoles can detect that the AP exists and they can attempt
to connect to it. This process is done by the ARM7 without any intervention by
the ARM9.

At any point you can call check the number of clients currently connected to
this host, as well as their MAC address and association ID. In general, you only
care about their association IDs (a number that goes from 1 to 15). DSWifi uses
this AID in several multiplayer-related functions.

```c
// This is all you normally need, the players mask. Each bit represents a
// connected player (bit 0 is the host, bits 1 to 15 represent clients with AIDs
// 1 to 15).
int num_clients = Wifi_MultiplayerGetNumClients();
u16 players_mask = Wifi_MultiplayerGetClientMask();
printf("Num clients: %d (mask 0x%02X)\n", num_clients, players_mask);
printf("\n");

// Normally you don't need to read the detailed client information, it's enough
// with the AID mask.
Wifi_ConnectedClient client[15]; // Up to 15 clients
num_clients = Wifi_MultiplayerGetClients(15, &(client[0]));

for (int i = 0; i < num_clients; i++)
{
    printf("AID %d (State %d) %04X:%04X:%04X\n",
           client[i].association_id,
           client[i].state,
           client[i].macaddr[0], client[i].macaddr[1], client[i].macaddr[2]);
}
```

Once you have enough clients you can prevent new connections by calling this:

```c
Wifi_MultiplayerAllowNewClients(false);
```

At any point, if you determine that a client is idle, or cheating, or anything,
you can call the following function to kick that client out of the host:

```c
Wifi_MultiplayerKickClientByAID(association_id);
```

You also need to setup a handler for received packets from clients. This is a
sample handler that can handle both multiplayer packets and regular data
packets:

```c
void FromClientPacketHandler(Wifi_MPPacketType type, int aid, int base, int len)
{
    printf("Client %d: ", aid);

    if (type == WIFI_MPTYPE_REPLY)
    {
        for (int i = 0; i < len; i += 2)
        {
            u16 data = 0;
            Wifi_RxRawReadPacket(base + i, sizeof(data), (void *)&data);
            printf("%04X ", data);
        }
        printf("\n");
    }
    else if (type == WIFI_MPTYPE_DATA)
    {
        if (len < 50)
        {
            char string[50];
            Wifi_RxRawReadPacket(base, len, &string);
            string[len] = 0;
            printf("%s", string);
        }
        printf("\n");
    }
}
```

Instead of using `Wifi_RxRawReadPacket(base, len, &buffer)` to copy the packet
to your own buffer you can use `Wifi_RxRawReadPacketPointer(base)`. This returns
a direct pointer to the data that you can use. However, this points to uncached
RAM, which is slow to read. It may be faster to copy the whole packet to your
own buffer in the stack (which is in DTCM, and it's very fast memory). Don't
convert the uncached pointer to a cached pointer! DSWiFi assumes that all the
memory in the buffer is uncached and it isn't prepared to manage the cache.

Call this to setup the handler:

```c
Wifi_MultiplayerFromClientSetPacketHandler(FromClientPacketHandler);
```

Finally, to send data to the clients you can do it like this:

```c
// Send multiplayer CMD packet that will trigger replies from clients
packet_host_to_client host_packet = { ... };
Wifi_MultiplayerHostCmdTxFrame(&host_packet, sizeof(host_packet));

// Send data packet to a single client with the specified association ID. This
// won't trigger automatic replies.
int association_id = 1;
const char *str = "Hey, this is some text";
Wifi_MultiplayerHostToClientDataTxFrame(association_id, str, strlen(str));
```

When you want to leave host mode, call this:

```c
Wifi_IdleMode();
```

After calling this, all clients will be disconnected from the host, and the
access point created by this console will stop being advertised.

## 4. Client consoles

Enter client mode:

```c
typedef struct {
    u32 response;
} packet_client_to_host;

// You can leave the packet size as 0 if you don't want to use multiplayer
// transfers.
Wifi_MultiplayerClientMode(sizeof(packet_client_to_host));

// Wait for the library to enter client mode
while (!Wifi_LibraryModeReady())
    swiWaitForVBlank();
```

The first step is to enter scan mode to look for access points. By default, only
access points with Nintendo-specific metadata will be saved, other access points
are ignored in multiplayer mode:

```c
Wifi_ScanMode();
```

Scan mode will iterate through all channels and add the access points to an
internal list kept by DSWifi. At the moment, access points of host consoles
using DSWifi don't use any encryption. You can check the list of detected access
points like this, for example, but note that you need to do this in an
interactive menu so that the user can wait for access points to show up:

```c
// Get find out how many APs there are in the area
int count = Wifi_GetNumAP();

printf("Number of AP: %d\n", count);
printf("\n");

for (int i = 0; i < count; i++)
{
    Wifi_AccessPoint ap;
    Wifi_GetAPData(i, &ap);

    printf("[%.24s] %s\n", ap.ssid);
    printf("Channel %2d | RSSI %u\n", ap.channel, ap.rssi);
    printf("Players %d/%d | %s\n",
           ap.nintendo.players_current, ap.nintendo.players_max,
           ap.nintendo.allows_connections ? "Open" : "Closed")
    printf("Game ID: %08X\n", (unsigned int)ap.nintendo.game_id);
    printf("\n");
}
```

As you can see, there is a lot of information in a ``Wifi_AccessPoint`` struct.
It contains the maximum number of clients that the host allows, the number of
clients connected currently, and whether this host allows new connections or
not. This is just a hint that can be ignored by clients (which can try to
connect to APs that are full and not allowing new connections, but the
connection will fail).

There are two more fields in the `Wifi_AccessPoint` structure that haven't been
used in the example. The user name of the host is stored in `ap.nintendo.name`,
and its length is stored in `ap.nintendo.name_len`. The name is stored in
UTF-16LE format by default and the default value is the name defined by the user
in the firmware of the DS. However, you can use `Wifi_MultiplayerHostName()` to
set the values of both fields to anything that you prefer. DSWifi doesn't use
the fields for anything, it just provides them to the user of the library, so
you can use any format you want (you can use an ASCII string if you don't want
to use UTF-16LE, for example).

When you have found an AP you want to connect to, run:

```c
Wifi_ConnectOpenAP(&ap);
```

Then, wait for the connection to be completed (or to fail!):

```c
while (1)
{
    swiWaitForVBlank();

    int status = Wifi_AssocStatus();

    if (status == ASSOCSTATUS_CANNOTCONNECT)
    {
        // We can't connect to this host, try to connect to a different one!
    }

    if (status == ASSOCSTATUS_ASSOCIATED)
    {
        // Success!
    }
}
```

Remember to call ``Wifi_AssocStatus()`` regularly to check if the connection
with the host has finished.

You also need to setup a handler for packets received from the host:

```c
void FromHostPacketHandler(Wifi_MPPacketType type, int base, int len)
{
    printf("Host (%d): ", len);

    if (type == WIFI_MPTYPE_REPLY)
    {
        for (int i = 0; i < len; i += 2)
        {
            u16 data = 0;
            Wifi_RxRawReadPacket(base + i, sizeof(data), (void *)&data);
            printf("%04X ", data);
        }
        printf("\n");
    }
    else if (type == WIFI_MPTYPE_DATA)
    {
        if (len < 50)
        {
            char string[50];
            Wifi_RxRawReadPacket(base, len, &string);
            string[len] = 0;
            printf("%s", string);
        }
        printf("\n");
    }
}
```

Call this to setup the handler:

```c
Wifi_MultiplayerFromHostSetPacketHandler(FromHostPacketHandler);
```

Finally, to send data to the host you can do it like this:

```c
// Prepare a REPLY packet that will be sent automatically when a CMD packet is
// received.
packet_client_to_host = { ... };
Wifi_MultiplayerHostReplyTxFrame(&host_packet, sizeof(host_packet));

// Send data packet to the host. This won't trigger automatic replies.
int association_id = 1;
const char *str = "Hey, this is some text";
Wifi_MultiplayerClientToHostDataTxFrame(association_id, str, strlen(str));
```

If you want to leave this AP, but remain in client mode:

```c
Wifi_DisconnectAP();
```

When you want to leave client mode, call this:

```c
Wifi_IdleMode();
```

After calling this, the client will send a packet to the host to notify that
it's leaving, and DSWifi will become idle.
