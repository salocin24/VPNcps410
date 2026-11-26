from ctypes import *

cdll.LoadLibrary("../libvpn.so")

# Constants
VPN_IFNAME_LEN = 16
VPN_KEY_LEN = 32

# VPN Tunnel Config struct

# Constructors for c-like array datatypes that are in the struct
ifname_type         = c_char * VPN_IFNAME_LEN
private_key_type    = c_uint8 * VPN_KEY_LEN

# ctypes struct implementation
class vpn_config_t(Structure):
    _fields_ = [("ifname", ifname_type),
                ("listen_port", c_uint16),
                ("private_key", private_key_type),
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