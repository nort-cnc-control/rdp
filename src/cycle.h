#pragma once

#include <defs.h>

enum rdp_state_e {
    RDP_CLOSED = 0,
    RDP_LISTEN,
    RDP_SYN_SENT,
    RDP_SYN_RCVD,
    RDP_OPEN,
    RDP_CLOSE_WAIT,
};

struct rdp_connection_s {
    enum rdp_state_e state;
    uint32_t cur_seq;
    uint32_t ack_seq;
    uint32_t rcv_seq;
    uint32_t rcv_data_seq;
    uint8_t *outbuf;
    size_t out_data_length;
    uint8_t local_port;
    uint8_t remote_port;
    void (*send)(const uint8_t *, size_t);
};

void rdp_init_connection(struct rdp_connection_s *conn, uint8_t *outbuf,
                         void (*send)(const uint8_t *, size_t),
                         void (*connected)(struct rdp_connection_s *),
                         void (*closed)(struct rdp_connection_s *));

bool rdp_listen(struct rdp_connection_s *conn, uint8_t port);

bool rdp_connect(struct rdp_connection_s *conn, uint8_t src_port, uint8_t dst_port);

bool rdp_close(struct rdp_connection_s *conn);

bool rdp_send(struct rdp_connection_s *conn, const uint8_t *data, size_t dlen);

bool rdp_received(struct rdp_connection_s *conn, const uint8_t *inbuf, uint8_t *data, size_t *dlen);

bool rdp_final_close(struct rdp_connection_s *conn);

bool rdp_has_ack(struct rdp_connection_s *conn);

bool rdp_retry(struct rdp_connection_s *conn);
