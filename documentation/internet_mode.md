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

If all you want to do is to use Internet mode you can use this to use the
additional DSi WiFi capabilities (like WPA2 support):

```c
Wifi_InitDefault(INIT_ONLY | WIFI_ATTEMPT_DSI_MODE);
```

Without `WIFI_ATTEMPT_DSI_MODE` the library will start in DS compatibility mode.

On the ARM7, hardware timer number `LIBNDS_DEFAULT_TIMER_WIFI` will be used by
the WiFi library after this call. On the ARM9, hardware timer number
`LIBNDS_DEFAULT_TIMER_WIFI` will also be used. Note that both numbers may be
different.

### 1.1 Connect to WFC settings

You can start autoconnect mode by calling:

```c
Wifi_AutoConnect();
```

In this case, skip the step of manually looking for access points.

### 1.2 Manually look for access points

If you don't want to autoconnect to WFC settings you can manually look for
access points like this, for example:

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

        // We get the name, channel, security type, and even a flag telling us
        // that this AP is configured in the WFC settings of the console.
        printf("[%.20s]%s\n", ap.ssid,
               (ap.flags & WFLAG_APDATA_CONFIG_IN_WFC) ? " WFC" : "");
        printf("%s | Channel %2d | RSSI %d\n",
               Wifi_ApSecurityTypeString(ap.security_type),
               ap.channel, ap.rssi);
        printf("\n");

        if (ap.flags & WFLAG_APDATA_COMPATIBLE)
        {
            // This AP is supported in the current execution mode. WPA2 networks
            // are only supported in DSi.

            // ...
        }
    }
}
```

After you have decided which AP to connect to:

```c
// Setting everything to 0 will make DHCP determine the IP address. You can also
// set the settings manually if you want.
Wifi_SetIP(0, 0, 0, 0, 0);

if (AccessPoint.flags & WFLAG_APDATA_CONFIG_IN_WFC)
{
    // If the AP is known, use the password stored in the WFC settings
    Wifi_ConnectWfcAP(&AccessPoint);
}
else
{
    // This AP isn't in the WFC settings. If the access point requires a WEP/WPA
    // password, ask the user to provide it. Note that you can still allow the
    // user to call this function with an AP that is saved in the WFC settings,
    // but the function will ignore the saved settings in the WFC configuration
    // and use the provided password instead.
    if (AccessPoint.security_type != AP_SECURITY_OPEN)
    {
        // WEP passwords can be 5 or 13 characters long, WPA passwords must be at
        // most 64 characters long.
        const char *password = "MyPassword123";
        Wifi_ConnectSecureAP(&AccessPoint, (u8 *)password, strlen(password));
    }
    else
    {
        // Open network
        Wifi_ConnectSecureAP(&AccessPoint, NULL, 0);
    }
}
```

`Wifi_ConnectSecureAP()` expects WEP keys as a string (usually called "ASCII"
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

It will free all RAM used by the library and the hardware timers. You can call
`Wifi_InitDefault()` at a later time to re-initialize it.

Important note: `Wifi_Deinit()` doesn't currently work after starting DSWiFi in
Internet mode.

### 1.5 Notes about APs with hidden SSID

APs generally support an option to "hide" the network. This means that they
don't openly broadcast their name, and they need to be probed before a client
can connect to them.

DSWiFi supports hidden networks that are configured in the WFC settings, and it
also supports connecting to networks if the user provides the SSID manually.

However, hiding the SSID isn't part of the WiFi standard.
[This blog post](https://goodwi.fi/posts/2023/12/hunt-for-hidden-probe/) goes
into more detail. The important part is:

> (CWSP-206 2019)
> SSID hiding is a technique implemented by WLAN device manufacturers that will
> remove the information found in the SSID information element from Beacon
> management frames. Depending on the implementation, SSID hiding may also
> remove it from Probe Response frames sent from the AP.

APs that don't provide their SSID in their probe responses aren't supported by
DSWiFi.

## 2. Get connection settings

You can get information like the IPv4 and IPv6 addresses of the console like
this:

```c
struct in_addr ip = { 0 }, gateway = { 0 }, mask = { 0 };
struct in_addr dns1 = { 0 }, dns2 = { 0 };
ip = Wifi_GetIPInfo(&gateway, &mask, &dns1, &dns2);

printf("\n");
printf("IPv4 information:\n");
printf("\n");
printf("IP:      %s\n", inet_ntoa(ip));
printf("Gateway: %s\n", inet_ntoa(gateway));
printf("Mask:    %s\n", inet_ntoa(mask));
printf("DNS1:    %s\n", inet_ntoa(dns1));
printf("DNS2:    %s\n", inet_ntoa(dns2));
printf("\n");
printf("IPv6 information:\n");
printf("\n");
char buf[128];
printf("IP: %s\n", inet_ntop(AF_INET6, &ipv6, buf, sizeof(buf)));
```

### 3. Use libc to access the Internet

### 3.1 Resolve host IPs

Function `gethostbyname()` is deprecated and it shouldn't be used, as it only
supports IPv4 addresses. `getaddrinfo()` is more flexible and it supports IPv4
and IPv6. It is more complicated, so BlocksDS comes with some examples that show
how to do it. This is the relevant part of the code:

```c
const char *url = "www.wikipedia.com";
const char *port = "80";

// This value will try to get IPv4 and IPv6 addresses. If you only want to get
// IPv4, use AF_INET. If you only want to use IPv6, use AF_INET6.
int family = AF_UNSPEC;

struct addrinfo hint;

hint.ai_flags = AI_CANONNAME;
hint.ai_family = family;
hint.ai_socktype = SOCK_STREAM; // TCP
hint.ai_protocol = 0;
hint.ai_addrlen = 0;
hint.ai_canonname = NULL;
hint.ai_addr = NULL;
hint.ai_next = NULL;

struct addrinfo *result, *rp;

int err = getaddrinfo(url, port, &hint, &result);
if (err != 0)
{
    printf("getaddrinfo(): %d\n", err);
    return;
}

struct addrinfo *found_rp = NULL;

for (rp = result; rp != NULL; rp = rp->ai_next)
{
    struct sockaddr_in *sinp;
    const char *addr;
    char buf[1024];

    printf("- Canonical Name:\n  %s\n", rp->ai_canonname);
    if (rp->ai_family == AF_INET)
    {
        // This should never happen if we have asked for IPv6 addresses
        printf("- AF_INET\n");

        sinp = (struct sockaddr_in *)rp->ai_addr;
        addr = inet_ntop(AF_INET, &sinp->sin_addr, buf, sizeof(buf));

        printf("  %s:%d\n", addr, ntohs(sinp->sin_port));

        if ((family == AF_INET) || (family == AF_UNSPEC))
        {
            found_rp = rp;
            break;
        }
    }
    else if (rp->ai_family == AF_INET6)
    {
        printf("- AF_INET6\n");

        sinp = (struct sockaddr_in *)rp->ai_addr;
        addr = inet_ntop(AF_INET6, &sinp->sin_addr, buf, sizeof(buf));

        printf("  [%s]:%d\n", addr, ntohs(sinp->sin_port));

        if ((family == AF_INET6) || (family == AF_UNSPEC))
        {
            found_rp = rp;
            break;
        }
    }
}

if (found_rp == NULL)
{
    printf("Can't find IP info!\n");
    freeaddrinfo(result);
    return;
}

printf("IP info found!\n");

int sfd = socket(found_rp->ai_family, found_rp->ai_socktype, found_rp->ai_protocol);
if (sfd == -1)
{
    perror("socket");
    freeaddrinfo(result);
    return;
}
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

Also, it is a good idea to check the value returned by `Wifi_AssocStatus()`
every now and then to see if the connection is lost at some point.
