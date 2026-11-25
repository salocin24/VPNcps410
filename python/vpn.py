from ctypes import *

cdll.LoadLibrary("../libvpn.so")

# Constants
VPN_KEY_LEN = 32
VPN_IFNAME_LEN = 16

# vpn tunnel config, holds a computers settings.
private_key_type = c_uint8 * VPN_KEY_LEN
class vpn_config_t(Structure):
    _fields_ = [("ifname", c_char * VPN_IFNAME_LEN),
                ("listen_port", c_uint16),
                ("private_key", private_key_type),
                ("debug", c_int)]

p_key = private_key_type(1, 2, 3, 4, 5, 6, 7)
config = vpn_config_t(b'ez0', 192, p_key, 0)
