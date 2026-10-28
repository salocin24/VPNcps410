#pragma once
#include <sys/types.h>

int tun_open(char *device);
void print_packet(const unsigned char *buffer, ssize_t length);