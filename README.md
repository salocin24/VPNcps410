# VPNcps410

A lightweight, open source VPN for Linux, built from the ground up for CPS 410 and modeled on WireGuard.

It's a C library (`libvpn.so`) that does the networking, with a Python GUI on top that controls it through `ctypes`.

## Status

**Working:** a plaintext tunnel between two machines. Packets go into the virtual interface (`ez0`), travel over UDP to the peer, and come out of the peer's `ez0`.

**Not yet:** encryption, the handshake, the GUI, and multiple peers. See the [Roadmap](#roadmap).

## How it works

```
 Machine A                                        Machine B
 app -> ez0 -> TX thread -> UDP ----LAN----> UDP -> RX thread -> ez0 -> app
 app <- ez0 <- RX thread <- UDP <---LAN----- UDP <- TX thread <- ez0 <- app
```

- **TUN device (`ez0`):** a virtual network interface. The kernel hands us every packet sent to the tunnel's addresses.
- **TX thread:** reads packets from `ez0` and sends them to the peer over UDP. Encryption will go here.
- **RX thread:** receives UDP packets from the peer and writes them into `ez0`. Decryption will go here.
- **Python GUI:** calls the functions in `vpn.h` to start and stop the tunnel and show stats.

## Files

| File | What it is |
|---|---|
| `vpn.h` | The public API, i.e. every function and struct Python can use. Start here. |
| `vpn.c` | The library: lifecycle, peers, and the TX/RX threads |
| `tun.c` / `tun.h` | Creates and attaches to the TUN device |
| `test_cli.c` | Command-line test program that uses the library without the GUI |
| `netns_test.sh` | Simulates two machines on one computer and pings through the tunnel |

## Requirements

- Linux (tested on Ubuntu under WSL)
- `gcc`, `iproute2`, and `ping`
- `sudo`, for creating the TUN device and running the namespace test

## Build

```
gcc -Wall -Werror -fPIC -shared -o libvpn.so vpn.c tun.c -lpthread
gcc -Wall -Werror -o test_cli test_cli.c -L. -lvpn -Wl,-rpath,.
```

`tun.c` can also be built on its own for testing the TUN device by itself:

```
gcc -Wall -DTUN_STANDALONE -o tun tun.c
```

## Test

```
chmod +x netns_test.sh
sudo ./netns_test.sh
```

This creates two network namespaces (fake machines at `192.168.50.1` and `192.168.50.2`), runs `test_cli` on each, and pings across the tunnel. A pass looks like `3 received, 0% packet loss`, with both logs ending in `stopped`.

To run the test program by hand:

```
sudo ./test_cli <ifname> <listen_port> <peer_ip> <peer_port>
# e.g.
sudo ./test_cli ez0 51820 192.168.50.2 51820
```

## Using the library (for the Python side)

Functions return `VPN_OK` (0) or a negative `VPN_ERR_*` code, and `vpn_strerror()` turns a code into a message.

Call them in this order:

1. `vpn_init(&cfg)`: settings for this machine (interface name, port, private key)
2. `vpn_add_peer(&peer)`: once per peer
3. `vpn_start()`: starts the tunnel and returns immediately
4. `vpn_get_stats(&stats)`: poll on a timer to update the GUI
5. `vpn_stop()`, then `vpn_cleanup()`

The structs in `vpn.h` must be mirrored exactly in Python with `ctypes.Structure`. **If you change `vpn.h`, update the Python side too.**

## Roadmap

- [x] TUN device
- [x] Library API (`vpn.h`)
- [x] TX/RX threads over UDP (plaintext)
- [ ] Makefile and `setup.sh` (run without sudo)
- [ ] `vpn.py` ctypes wrapper and GUI
- [ ] Encryption (ChaCha20-Poly1305 via libsodium)
- [ ] Handshake and key exchange
- [ ] Route packets to peers by tunnel IP
- [ ] Server-side forwarding/NAT (our modification of WireGuard)
- [ ] Testing on Raspberry Pi
