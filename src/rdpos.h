#pragma once

#include <rdp.h>
#include <serial-datagram/serial_datagram.h>

struct rdpos_cbs_s {
    void (*send_fn)(void *arg, const void *p, size_t len);
};

struct rdpos_connection_s 
{
    struct rdpos_cbs_s *cbs;
    serial_datagram_rcv_handler_t serial_handler;
    struct rdp_connection_s rdp_conn;
    void *user_arg;
};
