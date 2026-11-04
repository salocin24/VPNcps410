#!/bin/bash
# netns_test.sh - simulates two machines on one computer and pings through the tunnel.
# ns1 and ns2 are network namespaces connected by a virtual cable (veth pair),
# like two computers on a LAN at 192.168.50.1 and 192.168.50.2.
#
# run from the folder with libvpn.so and test_cli:  sudo ./netns_test.sh
set -e
# "|| true" so a namespace that doesn't exist yet isn't treated as an error by set -e
cleanup() { ip netns del ns1 2>/dev/null || true; ip netns del ns2 2>/dev/null || true; }
trap cleanup EXIT
cleanup

# the fake LAN
ip netns add ns1
ip netns add ns2
ip link add veth1 type veth peer name veth2
ip link set veth1 netns ns1
ip link set veth2 netns ns2
ip -n ns1 addr add 192.168.50.1/24 dev veth1 && ip -n ns1 link set veth1 up
ip -n ns2 addr add 192.168.50.2/24 dev veth2 && ip -n ns2 link set veth2 up

# start the VPN on each "machine", each pointing at the other
ip netns exec ns1 timeout -s INT 10 ./test_cli ez0 51820 192.168.50.2 51820 > ns1.log 2>&1 &
ip netns exec ns2 timeout -s INT 10 ./test_cli ez0 51820 192.168.50.1 51820 > ns2.log 2>&1 &
sleep 1

# tunnel addresses
ip -n ns1 addr add 10.99.0.1/24 dev ez0 && ip -n ns1 link set ez0 up
ip -n ns2 addr add 10.99.0.2/24 dev ez0 && ip -n ns2 link set ez0 up

# the actual test
ip netns exec ns1 ping -c 3 10.99.0.2

wait
echo "=== ns1 log";  cat ns1.log
echo "=== ns2 log";  cat ns2.log