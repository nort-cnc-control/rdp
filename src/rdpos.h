#pragma once

#include <rdp.h>
#include <serial-datagram/serial_datagram.h>

struct rdpos_cbs_s {
    void (*send_fn)(void *arg, const void *p, size_t len);
};

struct rdpos_connection_s 
{
    struct rdpos_cbs_s cbs;
    serial_datagram_rcv_handler_t serial_handler;
    struct rdp_connection_s *rdp_conn;
    void *user_arg;
};

void rdpos_init_connection(struct rdpos_connection_s *conn,
                           struct rdp_connection_s *rconn,
                           uint8_t *serial_receive_buf,
                           size_t serial_receive_buf_len);

void rdpos_set_send_cb(struct rdpos_connection_s *conn, void (*send)(void *, const void *, size_t));

bool rdpos_byte_received(struct rdpos_connection_s *conn, uint8_t data);
