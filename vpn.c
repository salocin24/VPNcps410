#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "vpn.h"
#include "tun.h"

#define MTU 1500

static vpn_config_t     g_cfg;              // copy the config from vpn_init()
static int              g_initialized = 0;  // has vpn_init() been called?
static atomic_int       g_running = 0;      // threads loop while this is 1
static int              g_tun_fd = -1;      // file descriptor from tun_open()
static int              g_sock_fd = -1;     // udp socket bound to listen_port
static pthread_t        g_tx_thread;

// internal peer record, the vpn_peer_t from python, plus the
// endpoint converted to a sockaddr_in so sendto() can use it directly
typedef struct {
    vpn_peer_t          info;
    struct sockaddr_in  addr;
    int                 has_endpoint;
} peer_entry_t;

static peer_entry_t     g_peers[VPN_MAX_PEERS];
static int              g_peer_count = 0;
static pthread_mutex_t  g_peers_lock = PTHREAD_MUTEX_INITIALIZER; //peers can be added while running!

static vpn_stats_t      g_stats;            // counters the gui polls
static pthread_mutex_t  g_stats_lock = PTHREAD_MUTEX_INITIALIZER;

// tx thread: tun -> "encrypt" -> UDP to peer
// basically the old main() loop from tun.c, moved into a thread

// g_ is suppose to designate a global that lives outside any function and
// lives while the library is loaded, ike g_cfg, g_running, and g_tun_fd.

static void *tx_thread(void *arg) {
    (void)arg; // unused?
    unsigned char buffer[MTU];
    struct pollfd pfd = {
        .fd = g_tun_fd,
        .events = POLLIN
    };

    while (g_running) {
        // read() would block forever if no packets come in, and then
        // vpn_stop() could never get this thread to exit. poll() waits at most
        // 500ms, so the loop gets a chance to recheck g_running.
        int ready = poll(&pfd, 1, 500);
        if (ready < 0) {
            if (errno == EINTR)
                continue;
            perror("poll");
            break;
        }
        if (ready ==0 )
            continue; // no packet yet
        
        ssize_t length = read(g_tun_fd, buffer, sizeof(buffer));
        if (length < 0) {
            if (errno == EINTR)
                continue;
            perror("read");
            break;
        }

        if (g_cfg.debug)
            print_packet(buffer, length);
        
        // todo: encrypt the packet before sending
        // todo: pcik the peer by the packets destination tunnel ip
        // for now everyhting goes to peer 0

        // copy the address while holding the lock, then send without it
        // so a slow sendto() never blocks vpn_add_peer()
        struct sockaddr_in dest;
        int have_dest = 0;
        pthread_mutex_lock(&g_peers_lock);
        if (g_peer_count > 0 && g_peers[0].has_endpoint) {
            dest = g_peers[0].addr;
            have_dest = 1;
        }
        pthread_mutex_unlock(&g_peers_lock);
        
        // drop a packet
        if (!have_dest) {
            pthread_mutex_lock(&g_stats_lock);
            g_stats.dropped++;
            pthread_mutex_unlock(&g_stats_lock);
            continue;
        }

        // one tun packet = one udp datagram
        if (sendto(g_sock_fd, buffer, length, 0,
                    (struct sockaddr*)&dest, sizeof(dest)) < 0) {
            perror("sendto");
            pthread_mutex_lock(&g_stats_lock);
            g_stats.dropped++;
            pthread_mutex_unlock(&g_stats_lock);
            continue;
        }

        // the gui reads g_stats from another thread, so lock while updating
        pthread_mutex_lock(&g_stats_lock);
        g_stats.tx_packets++;
        g_stats.tx_bytes += length;
        pthread_mutex_unlock(&g_stats_lock);
    }
    return NULL;
}

// lifecycle

int vpn_init(vpn_config_t *cfg) {
    if (cfg == NULL) {
        return VPN_ERR_ARGS;
    }
    if (g_running) {
        return VPN_ERR_STATE;
    }

    // copy cfg to g_cfg so the callers struct (cfg which is filled from the python call)
    // can terminate afterward
    g_cfg = *cfg;
    g_cfg.ifname[VPN_IFNAME_LEN - 1] = '\0'; //make sure the name is term

    memset(&g_stats, 0, sizeof(g_stats));
    g_initialized = 1;
    return VPN_OK;
}

int vpn_start(void) {
    if(!g_initialized || g_running) {
        return VPN_ERR_STATE;
    }

    g_tun_fd = tun_open(g_cfg.ifname);
    if (g_tun_fd < 0) {
        return VPN_ERR_TUN;
    }

    // udp socket, where encrypted packets travel between machines
    g_sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_sock_fd < 0) {
        perror("socket");
        close(g_tun_fd);
        g_tun_fd = -1;
        return VPN_ERR_SOCKET;
    }
    // bind to listen port on every local addr so peers can reach
    struct sockaddr_in local;
    memset(&local, 0, sizeof(local));
    local.sin_family        = AF_INET;
    local.sin_addr.s_addr   = htonl(INADDR_ANY);
    local.sin_port          = htons(g_cfg.listen_port);

    if (bind(g_sock_fd, (struct sockaddr *)&local, sizeof(local)) < 0) {
        perror("bind");
        close(g_sock_fd);
        close(g_tun_fd);
        g_sock_fd = g_tun_fd = -1;
        return VPN_ERR_SOCKET;
    }

    g_running = 1;
    if (pthread_create(&g_tx_thread, NULL, tx_thread, NULL) != 0) {
        g_running = 0;
        close(g_sock_fd);
        close(g_tun_fd);
        g_sock_fd = g_tun_fd = -1;
        return VPN_ERR_THREAD;
    }

    return VPN_OK;
}

int vpn_stop(void) {
    if (!g_running) {
        return VPN_ERR_STATE;
    }

    g_running = 0;                  // tell the thread to finish
    pthread_join(g_tx_thread, NULL); // wait for it to join

    close(g_sock_fd);
    close(g_tun_fd);
    g_sock_fd = g_tun_fd = -1;
    return VPN_OK;
}

int vpn_is_running(void) {
    if (g_running) {
        return 1;
    } else {
        return 0;
    }
}

void vpn_cleanup(void) {
    if (g_running) {
        vpn_stop();
    }
    memset(&g_cfg, 0, sizeof(g_cfg)); // wipes privat key from memory

    pthread_mutex_lock(&g_peers_lock);
    memset(g_peers, 0, sizeof(g_peers));
    g_peer_count = 0;
    pthread_mutex_unlock(&g_peers_lock);

    g_initialized = 0;
}

// peers section
int vpn_add_peer(const vpn_peer_t *peer) {
    if (peer == NULL) {
        return VPN_ERR_ARGS;
    }

    peer_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    // dereference btw
    entry.info = *peer;
    entry.info.endpoint_ip[VPN_ADDR_LEN - 1] = '\0';
    entry.info.tunnel_ip[VPN_ADDR_LEN - 1] = '\0';

    // convert an ipv4 + port into a sockaddr_in once
    // instead of every packet:
    if (entry.info.endpoint_ip[0] != '\0') {
        entry.addr.sin_family = AF_INET;
        entry.addr.sin_port = htons(entry.info.endpoint_port);
        // inet_pton = "presentation to network, reverse of inet_ntop in tun.c"
        if (inet_pton(AF_INET, entry.info.endpoint_ip, &entry.addr.sin_addr) != 1) {
            return VPN_ERR_ARGS; // not a valid ipv4
        }
        entry.has_endpoint = 1;
    }

    pthread_mutex_lock(&g_peers_lock);
    if (g_peer_count >= VPN_MAX_PEERS) {
        pthread_mutex_unlock(&g_peers_lock);
        return VPN_ERR_PEER_FULL;
    }
    for (int i = 0; i < g_peer_count; i++) {
        if (memcmp(g_peers[i].info.public_key, peer->public_key, VPN_KEY_LEN) == 0) {
            pthread_mutex_unlock(&g_peers_lock);
            return VPN_ERR_PEER_EXIST;
        }
    }
    g_peers[g_peer_count++] = entry;
    pthread_mutex_unlock(&g_peers_lock);
    return VPN_OK;
}

int vpn_get_peer_count(void) {
    pthread_mutex_lock(&g_peers_lock);
    int n = g_peer_count;
    pthread_mutex_unlock(&g_peers_lock);
    return n;
}

int vpn_get_stats(vpn_stats_t *out) {
    if (out == NULL) {
        return VPN_ERR_ARGS;
    }
    pthread_mutex_lock(&g_stats_lock);
    *out = g_stats;
    pthread_mutex_unlock(&g_stats_lock);
    return VPN_OK;
}

// errors

const char *vpn_strerror(int err) {
    switch (err) {
        case VPN_OK:             return "ok";
        case VPN_ERR_ARGS:       return "bad or missing argument";
        case VPN_ERR_STATE:      return "function called in the wrong order";
        case VPN_ERR_TUN:        return "could not open TUN device";
        case VPN_ERR_SOCKET:     return "could not create or bind UDP socket";
        case VPN_ERR_THREAD:     return "could not start thread";
        case VPN_ERR_PEER_FULL:  return "peer list is full";
        case VPN_ERR_PEER_EXIST: return "peer key or tunnel IP already in use";
        case VPN_ERR_NO_PEER:    return "no such peer";
        case VPN_ERR_CRYPTO:     return "crypto failure";
        default:                 return "unknown error";
    }
}