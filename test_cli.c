// test_cli.c runs libvpn.so from the terminal, useful for testing 
// before our gui is developed. it calls the library in the same order the 
// python GUI will.

// build: gcc -Wall -o test_cli test_cli.c -L. -lvpn -Wl,-rpath,.
// run sudo ./test_cli <ifname> <listen_port> <peer_ip> <peer_port>
//          ex: sudo ./test_cli ez0 51820 192.168.50.2 51820
//          then in another terminal:
//              sudo ip addr add 10.99.0.1/24 dev ez0
//              sudo ip link set ez0 up
//              ping 10.99.0.2
//              ctrl c to stop

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "vpn.h"

static volatile sig_atomic_t quit = 0;
static void on_sigint(int sig) { (void)sig; quit = 1; }

int main(int argc, char **argv) {
    if (argc < 5) {
        printf("usage: %s <ifname> <listen_port> <peer_ip> <peer_port>\n", argv[0]);
        return 1;
    }


    // 1. config for this machine
    vpn_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    strncpy(cfg.ifname, argv[1], VPN_IFNAME_LEN - 1);
    cfg.listen_port = (uint16_t)atoi(argv[2]);
    cfg.debug = 1;

    int err = vpn_init(&cfg);
    if (err != VPN_OK) {
        printf("vpn_init: %s\n", vpn_strerror(err));
        return 1;
    }

    // 2. one peer (no keys yet, so public_key is all zeros)
    vpn_peer_t peer;
    memset(&peer, 0, sizeof(peer));
    strncpy(peer.endpoint_ip, argv[3], VPN_ADDR_LEN - 1);
    peer.endpoint_port = (uint16_t)atoi(argv[4]);

    err = vpn_add_peer(&peer);
    if (err != VPN_OK) {
        printf("vpn_add_peer: %s\n", vpn_strerror(err));
        return 1;
    }

    err = vpn_start();
    if (err != VPN_OK) {
        printf("vpn_start: %s\n", vpn_strerror(err));
        return 1;
    }
    printf("running on %s, Crtl+C to stop\n", cfg.ifname);

    signal(SIGINT, on_sigint);

    // temp gui timer poll mimicry
    while (!quit) {
        sleep(2);
        vpn_stats_t s;
        vpn_get_stats(&s);
        printf("    tx: %llu pkts %llu bytes | rx: %llu pkts %llu bytes | dropped: %llu\n",
                (unsigned long long)s.tx_packets, (unsigned long long)s.tx_bytes,
                (unsigned long long)s.rx_packets, (unsigned long long)s.rx_bytes,
                (unsigned long long)s.dropped);
    }

    vpn_stop();
    vpn_cleanup();
    printf("stopped\n");
    return 0;
}