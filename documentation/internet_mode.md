# Internet mode guide

DSWifi supports Internet communications. However, DSi mode WiFi isn't supported
yet, so it is restricted to open and WEP-encrypted networks. DSWifi includes the
library lwIP to handle connections. It supports IPv4, as well as TCP and UDP.

## 1. Initialization

You can initialize the library in autoconnect mode or in manual connection mode.
With autoconnect mode DSWifi will check the WiFi networks stored in the firmware
of the console and it will try to connect to them:

```c
Wifi_InitDefault(WFC_CONNECT);
```

However, this isn't very flexible. The function will simply wait until the
connection works (or until the library times out). It will also make it
impossible to switch between local multiplayer and Internet modes.

It's generally better to initialize the library in manual mode and to give the
user the option to do different things. Initialize the library with this (it
starts by default in Internet mode, not multiplayer mode):

```c
Wifi_InitDefault(INIT_ONLY);
```

Hardware timer 3 will be used by the WiFi library after this call.

### 1.1 Connect to WFC settings

You can start autoconnect mode by calling:

```c
Wifi_AutoConnect();
```

### 1.2 Manually look for access points

You can manually look for access points like this, for example:

```c
// Set the library in scan mode
Wifi_ScanMode();

while (1)
{
    cothread_yield_irq(IRQ_VBLANK);

    // Get find out how many APs there are in the area
    int count = Wifi_GetNumAP();

    printf("Number of AP: %d\n", count);
    printf("\n");

    for (int i = 0; i < count; i++)
    {
        Wifi_AccessPoint ap;
        Wifi_GetAPData(i, &ap);

        // WPA isn't supported!

        printf("[%.24s]\n", ap.ssid)
        printf("%s | Channel %2d | RSSI %d\n",
               Wifi_ApSecurityTypeString(ap.security_type),
               ap.channel, ap.rssi);
        printf("\n");
    }
}
```

After you have decided which AP to connect to:

```c
// Setting everything to 0 will make DHCP determine the IP address. You can also
// set the settings manually if you want.
Wifi_SetIP(0, 0, 0, 0, 0);

// If the access point requires a WEP password, ask the user to provide it
if (AccessPoint.flags & WFLAG_APDATA_WEP)
{
    // WEP passwords can be 5 or 13 characters long
    const char *password = "MyPassword123";
    Wifi_ConnectSecureAP(&AccessPoint, (u8 *)password, strlen(password));
}
else
{
    Wifi_ConnectAP(&AccessPoint, WEPMODE_NONE, 0, 0);
}
```

`Wifi_DisconnectAP()` expects WEP keys as a string (usually called "ASCII"
key). If you have an hexadecimal key you have to save it as a string of
hexadecimal values. For example, `6162636465` needs to be passed to the function
as `{ 0x61, 0x62, 0x63, 0x64, 0x65 }`, not as `"6162636465"`.

### 1.3 Wait until you connect to the AP

Regardless of which mode you used to start the connection to the AP, wait for
the connection to be completed (or to fail!):

```c
while (1)
{
    cothread_yield_irq(IRQ_VBLANK);

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

### 1.4 Disconnect from the AP

Eventually, when you want to leave this AP:

```c
Wifi_DisconnectAP();
```

If you are done using wireless mode you can disable it to save power by calling:

```c
Wifi_DisableWifi();
```

If you aren't going to use DSWiFi again and you want to free up even more
resources, you can call:

```c
Wifi_Deinit();
```

It will free all RAM used by the library and hardware timer 3. You can call
`Wifi_InitDefault()` at a later time to re-initialize it.

## 2. Get connection settings

You can get information like the IP address like this:

```c
struct in_addr ip, gateway, mask, dns1, dns2;
ip = Wifi_GetIPInfo(&gateway, &mask, &dns1, &dns2);

printf("\n");
printf("Connection information:\n");
printf("\n");
printf("IP:      %s\n", inet_ntoa(ip));
printf("Gateway: %s\n", inet_ntoa(gateway));
printf("Mask:    %s\n", inet_ntoa(mask));
printf("DNS1:    %s\n", inet_ntoa(dns1));
printf("DNS2:    %s\n", inet_ntoa(dns2));
printf("\n");
```

### 3. Use libc to access the Internet

### 3.1 Resolve host IPs

You can obtain the IP of a host like this:

```c
const char *url = "www.wikipedia.com";

struct hostent *host = gethostbyname(url);

if (host)
    printf("IP: %s\n", inet_ntoa(*(struct in_addr *)host->h_addr_list[0]));
else
    printf("Could not get IP\n");
```

### 3.2 Communicate using sockets

You can create TCP or UDP sockets with:

```c
socket(AF_INET, SOCK_STREAM, 0); // TCP
socket(AF_INET, SOCK_DGRAM, 0); // UDP
```

Then, you can use standard socket functions to use it, like `connect()`,
`write()`, `read()`, `send()`, `recv()`, `shutdown()` and `close()`.

Note that, by default, sockets start in blocking mode. You can switch to
non-blocking mode with:

```c
int opt = 1;
int rc = ioctl(my_socket, FIONBIO, (char *)&opt);
```

If you use non-blocking sockets, remember to call `cothread_yield()` or
`cothread_yield_irq()` in your loop to give the socket thread CPU time.
