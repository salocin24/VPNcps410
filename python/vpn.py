from ctypes import *

libvpn = CDLL("../libvpn.so")

# Constants
VPN_IFNAME_LEN  = 16
VPN_KEY_LEN     = 32
VPN_ADDR_LEN    = 16
VPN_MAX_PEERS   = 16

# Custom C array types
ifname_type         = c_char * VPN_IFNAME_LEN
key_type            = c_uint8 * VPN_KEY_LEN
public_key_type     = c_uint8 * VPN_KEY_LEN
ip_address_type     = c_char * VPN_ADDR_LEN

# VPN Tunnel Config struct
class vpn_config_t(Structure):
    _fields_ = [("ifname", ifname_type),
                ("listen_port", c_uint16),
                ("private_key", key_type),
                ("debug", c_int)]

""" Creating ifname variables for the struct:
To pass an ifname to the struct it must be a string of bytes, 
which prepending a 'b' before the string will do.

config = vpn_config_t(b"ez0", 192, p_key, 0)

Or

ifn = b"ez0"
config = vpn_config_t(ifn, 192, p_key, 0)
"""

""" Creating private_key variables for the struct:
This will create a private key with only the first 5 values filled,
indexes 5 to 31 will be filled with zeroes. 

p_key = private_key_type(0, 1, 2, 3, 4)

Alternatively, if you have an array with the values, you can use Python's unpacking
operator to initialize the private_key.

test = [0, 1, 2, 3, 4]
p_key = private_key_type(*test)
"""

# VPN Peer struct
class vpn_peer_t(Structure):
    _fields_ = [("public_key", public_key_type),
                ("endpoint_ip", ip_address_type),
                ("endpoint_port", c_uint16),
                ("tunnel_ip", ip_address_type),
                ("keepalive_secs", c_uint16)]

class vpn_stats_t(Structure):
    _fields_= [("tx_packets", c_uint64),
               (("tx_bytes", c_uint64)),
               (("rx_packets", c_uint64)),
               (("rx_bytes", c_uint64)),
               (("dropped", c_uint64))]

class vpn_peer_status_t(Structure):
    _fields_ = [("public_key", key_type),
                ("tunnel_ip", ip_address_type),
                ("endpoint_ip", ip_address_type),
                ("endpoint_port", c_uint16),
                ("last_handshake", c_int64),
                ("tx_bytes", c_uint64),
                ("rx_bytes", c_uint64),]

# Declare function prototypes to make sure
# they will throw errors if called incorrectly
vpn_init = libvpn.vpn_init
vpn_init.argtypes = [vpn_config_t]
vpn_init.restype = c_int

vpn_start = libvpn.vpn_start
vpn_start.argtypes = []
vpn_start.restype = c_int

vpn_stop = libvpn.vpn_stop
vpn_stop.argtypes = []
vpn_stop.restype = c_int

vpn_is_running = libvpn.vpn_is_running
vpn_is_running.argtypes = []
vpn_is_running.restype = c_int

vpn_cleanup = libvpn.vpn_cleanup
vpn_cleanup.argtypes = []
vpn_cleanup.restype = None

vpn_add_peer = libvpn.vpn_add_peer
vpn_add_peer.argtypes = [vpn_peer_t]
vpn_add_peer.restype = c_int

vpn_remove_peer = libvpn.vpn_remove_peer
vpn_remove_peer.argtypes = [key_type]
vpn_remove_peer.restype = c_int

vpn_get_stats = libvpn.vpn_get_stats
vpn_get_stats.argtypes = [vpn_stats_t]
vpn_get_stats.restype = c_int

vpn_get_peer_count = libvpn.vpn_get_peer_count
vpn_get_peer_count.argtypes = []
vpn_get_peer_count.restype = c_int

vpn_get_peer_status = libvpn.vpn_get_peer_status
vpn_get_peer_status.argtypes = [c_int, vpn_peer_status_t]
vpn_get_peer_status.restype = c_int