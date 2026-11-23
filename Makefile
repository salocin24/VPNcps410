CC      = gcc
CFLAGS  = -Wall -Werror

LIB_SRCS = vpn.c tun.c
LIB_OBJS = $(LIB_SRCS:.c=.o)

all: libvpn.so test_cli

%.o: %.c vpn.h tun.h
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

libvpn.so: $(LIB_OBJS)
	$(CC) -shared -o $@ $^ -lpthread

test_cli: test_cli.c vpn.h libvpn.so
	$(CC) $(CFLAGS) -o $@ test_cli.c -L. -lvpn -Wl,-rpath,.

tun: tun.c tun.h
	$(CC) -Wall -DTUN_STANDALONE -o $@ tun.c

test: all
	sudo bash ./netns_test.sh

clean:
	rm -f *.o libvpn.so test_cli tun ns1.log ns2.log

.PHONY: all test clean