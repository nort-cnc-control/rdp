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

struct rdpos_buffer_set_s
{
    uint8_t *rdp_outbuf;
    size_t rdp_outbuf_len;

    uint8_t *rdp_recvbuf;
    size_t rdp_recvbuf_len;

    uint8_t *serial_receive_buf;
    size_t serial_receive_buf_len;
};

void rdpos_init_connection(struct rdpos_connection_s *conn,
                           struct rdpos_buffer_set_s *bufs,
                           struct rdp_cbs_s *rdp_cbs,
                           struct rdpos_cbs_s *rdpos_cbs);

bool rdpos_byte_received(struct rdpos_connection_s *conn, uint8_t data);
