// tun.c -  creating a TUN interface (vnic) and printing the IPv4 headers of packets read from it.
// build:   gcc -Wall -o tun tun.c
// Run:     sudo ./tun ez0

// when running, you should only see device traffic sent to the 10.99.0.1/24 (or whatever subnet you gave)
// not something like google, since the command ip addr add 10.99.0.1/24 dev ez0, the kernel auto added a route in the routing table meaning, 
// "anything for the addres 10.99.0.0/24 goes out ez0" google's ip doesn't match that, so it would use your default route (the normal interface) and ez0 wouldn't see it.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <arpa/inet.h>
#include <netinet/ip.h>

#define MTU 1500

int tun_open(char *device);
void print_packet(const unsigned char *buffer, ssize_t length);

int main(int argc, char **argv) {
    char device[16] = "ez0";
    if (argc > 1) {
        // this just copies a custom interface name into the dev array, max of 15 bytes. 15 since strncpy doesn't
        // include a null terminator like a normal char array initialization would.
        strncpy(device, argv[1], 16 - 1);
    }

    // our file descriptor, used a lot for read(), write(), and close() stuff.
    int fd = tun_open(device);
    if (fd < 0)
        return 1;

    printf("Opened TUN device %s. Bring it up in another terminaland do:\n", device);
    printf("    sudo ip addr add 10.99.0.1/24 dev %s\n", device);
    printf("    sudo ip link set %s up\n", device);
    printf("    ping 10.99.0.2\n\n");

    unsigned char buffer[MTU];
    while (1) {
        ssize_t length = read(fd, buffer, sizeof(buffer));
        if (length < 0) {
            if (errno == EINTR)
                continue;
            perror("read");
            break;
        }
        print_packet(buffer, length);
    }

    close(fd);
    return 0;
}

int tun_open(char *device) {
    int fd = open("/dev/net/tun", O_RDWR);
    if (fd < 0) {
        perror("open dev/net/tun");
        return -1;
    }

    struct ifreq interfaceRequest;
    memset(&interfaceRequest, 0, sizeof(interfaceRequest));
    interfaceRequest.ifr_flags = IFF_TUN | IFF_NO_PI; // raw ip packets, no extra headers

    if(*device)
        // similar stuff as before in main
        strncpy(interfaceRequest.ifr_name, device, 16 - 1);

    // TUNSETIFF is a ioctl command code that means "set up the TUN interface" it tells the kernel to take the 
    // name and flags in interfaceRequest and attach this filedescriptor (fd) to a new interface given
    // obviously, ioctl() is still is called an is successfull if this error block does not evaluate.
    if(ioctl(fd, TUNSETIFF, &interfaceRequest) < 0) {
        perror("ioctl TUNSETIFF");
        close(fd);
        return -1;
    }

    strcpy(device, interfaceRequest.ifr_name);
    return fd;
}

static const char *proto_name(int p) {
    // simple usecase function to determine what protocol is inside the packet.
    switch (p) {
        case IPPROTO_ICMP:  return "ICMP";
        case IPPROTO_TCP:   return "TCP";
        case IPPROTO_UDP:   return "UDP";
        default:            return "other";
    }
}

void print_packet(const unsigned char *buffer, ssize_t length) {
    // this little line determines which ip version is being recieved, IPv4 or IPv6.
    // The first byte of every IP packet holds two values packed together:
    //      - high 4 bits: the version (4 for ipv4, 6 for ipv6)
    //      - and low 4 bits: the HEADER length (ipv4 only)
    // >> 4 shifts the byte being read right by 4 bits, which drops the low half and leaves just the version.
    // example: for a typical ipv4 packet, the first byte is 0x45 (0100 0101). when we do a bitshift >> 4, we get 0000 0100 which equals 4, our version.
    // example: for a typical ipv6 packet, the frist byte is 0x6_ , and when we do the same operation, we get a 6 returned.
    int version = buffer[0] >> 4;

    if (version != 4) {
        printf("[%zd bytes] IPv%d packet (skipping)\n", length, version);
        return;
    }
    if (length < (ssize_t)sizeof(struct iphdr)) {
        printf("[%zd bytes] truncated IPv4 packet\n", length);
        return;
    }
    // this line lets u read the raw packet bytes as an ip header with named fields.
    // since buffer is just an array of bytes, struct iphdr (from ip.h) describes the ipv4 header layout: version, ttl, protocol, saddr, daddr, etc, 
    // each at the correct byte offset. The line casts the buffer's address to a struct iphdr * that says "treat the bytes here as an ip header" After that
    // you can write ip->ttl or ip->saddr instead of doing it manually.
    const struct iphdr *ip = (const struct iphdr *)buffer;

    //This converts the source and destination IPs from raw binary into readable strings like "192.168.50.1"
    // the below char arrays make buffers big enough to hold the longest IPv4 string.
    // inet_ntop() stands for "network to presentation" It takes the 4-byte binary address and writes the dotted-decimal text into your buffer. Its args are:
    // AF_INET:     the address is IPv4 (AF_INET6 would mean ipv6)
    // &ip->saddr:  a pointer to the source 4 address bytes in the packet
    // &ip->daddr:  a pointer to the destination 4 address bytes in the packet
    // src:         where to write the string
    // sizeof(src): the buffer size, so it can't overflow
    char src[INET_ADDRSTRLEN], dst[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ip->saddr, src, sizeof(src));
    inet_ntop(AF_INET, &ip->daddr, dst, sizeof(dst));

    printf("[%zd bytes] IPv4 %s -> %s proto=%s ttl=%d\n",
            length, src, dst, proto_name(ip->protocol), ip->ttl);
}
