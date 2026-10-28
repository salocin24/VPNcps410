// vpn.h used for libvpn.so
// used for the inbetween for the C backend and the Python GUI (ctypes)
// the python GUI will call each function in the following order:
// vpn_init(&cfg)       1. hand over settings (TUN, name, port, private key)
// vpn_add_peer(&peer)  2. Tell it who its allowed to talk to
// vpn_start()          3. Open TUN, bind UDP,  spawn threads, return instantly so it doesn't hang on a process
// vpn_get_stats()      4. GUI polls these on a timer to update the display
// vpn_stop()           5. stop the threads, close the tunnel and socket
// vpn_cleanup()        6. wipe keys and the peer list from memory

// usefulness:
// The person writing vpn.py reads vpn.h to know exactly which structs 
// and functions to mirror in ctypes. They don’t have to dig through hundreds of 
// lines of vpn.c. That means people can start on the Python side before vpn.c 
// is even written.

// tx threads - packaging/sending
// rx threads - recieve

#pragma once

#include <stdint.h>

// sizes for specific thresholds
#define VPN_KEY_LEN     32  // same size wireguard uses for keys
#define VPN_IFNAME_LEN  16  // matches IFNAMSIZ, used in our tun.c device[16] field
#define VPN_ADDR_LEN    16  // IPv4 dotted decimal string + null term ("255.255.255.255")
#define VPN_MAX_PEERS   16

// return codes
#define VPN_OK               0
#define VPN_ERR_ARGS        -1 // missing argument
#define VPN_ERR_STATE       -2 // wrong order of functions called
#define VPN_ERR_TUN         -3 // tun_open() failed 
#define VPN_ERR_SOCKET      -4 // UDP socket failed
#define VPN_ERR_THREAD      -5 //pthread_create failed
#define VPN_ERR_PEER_FULL   -6 // already at max vpn peers
#define VPN_ERR_PEER_EXIST  -7 // public key or tunnel ip already in use
#define VPN_ERR_NO_PEER     -8 // no peer with that key /index
#define VPN_ERR_CRYPTO      -9 // key gen or crypto failure

// vpn tunnel config, holds a computers settings.
// gets filled on the python side (like all of these structs)
typedef struct {
    char        ifname[VPN_IFNAME_LEN];     // tunnel device to attatch to. "ez0" for example
    uint16_t    listen_port;                // UDP port to listen on, like 6969
    uint8_t     private_key[VPN_KEY_LEN];   // this computers private key
    int         debug;                      // nonzero = print_packet() every packet
} vpn_config_t;

// a peer to add
typedef struct {
    uint8_t     public_key[VPN_KEY_LEN];    // identifies the peer
    char        endpoint_ip[VPN_ADDR_LEN];  // "" if unknown (they connect to us)
    uint16_t    endpoint_port;              // "" = derive hidden 10.99.x.y from public key
    char        tunnel_ip[VPN_ADDR_LEN];        
    uint16_t    keepalive_secs;             // 0 = off
} vpn_peer_t;

// tunnel counters for the GUI stats page
typedef struct {
    uint64_t    tx_packets;
    uint64_t    tx_bytes;
    uint64_t    rx_packets;
    uint64_t    rx_bytes;
    uint64_t    dropped;
} vpn_stats_t;

// per peer status
typedef struct {
    uint8_t     public_key[VPN_KEY_LEN];
    char        tunnel_ip[VPN_ADDR_LEN];
    char        endpoint_ip[VPN_ADDR_LEN];
    uint16_t    endpoint_port;
    int64_t     last_handshake;             // time in seconds, 0 = never
    uint64_t    tx_bytes;
    uint64_t    rx_bytes;
} vpn_peer_status_t;

// keys. useless until we add crypto stuff
int vpn_generate_keypair(uint8_t private_key[VPN_KEY_LEN], uint8_t public_key[VPN_KEY_LEN]);

// lifecylce
int vpn_init(vpn_config_t *cfg);    //stores a passed in config struct
int vpn_start(void);                // tun_open + bind UDP + spawn threads
int vpn_stop(void);                 // signal threads, join them, close file descriptors
int vpn_is_running(void);           // 1 = running, 0 = stopped
void vpn_cleanup(void);             // wipe keys and peers; call after vpn_stop

// peers 
int vpn_add_peer(const vpn_peer_t *peer);
int vpn_remove_peer(const uint8_t public_key[VPN_KEY_LEN]);

// status
int vpn_get_stats(vpn_stats_t *out);
int vpn_get_peer_count(void);
int vpn_get_peer_status(int index, vpn_peer_status_t *out);

// errors
const char *vpn_strerror(int err);