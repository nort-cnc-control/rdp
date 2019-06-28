#pragma once

#include <defs.h>

enum rdp_state_e {
    RDP_CLOSED = 0,
    RDP_LISTEN = 1,
    RDP_SYN_SENT = 2,
    RDP_SYN_RCVD = 3,
    RDP_OPEN = 4,
    RDP_ACTIVE_CLOSE_WAIT = 5,
    RDP_PASSIVE_CLOSE_WAIT = 6,
};

struct rdp_connection_s;

struct rdp_cbs_s {
    void (*send)(struct rdp_connection_s *, const uint8_t *, size_t);
    void (*connected)(struct rdp_connection_s *);
    void (*closed)(struct rdp_connection_s *);
    void (*data_send_completed)(struct rdp_connection_s *);
    void (*data_received)(struct rdp_connection_s *, const uint8_t *, size_t);
};

struct rdp_connection_s {
    //    The current state of the connection.  Legal values are OPEN,
    //    LISTEN, CLOSED, SYN-SENT, SYN-RCVD,  and CLOSE-WAIT.
    enum rdp_state_e state;

    struct {
        // The sequence number of the next segment that is to be sent.
        uint32_t nxt;

        // The sequence number of the oldest unacknowledged segment.
        uint32_t una;

        // The initial send sequence  number.
        uint32_t iss;

        // The sequence number of the last package with data
        uint32_t dts;
    } snd;

    struct {
        // The sequence number of the last segment  received  correctly and in sequence.
        uint32_t cur;

        // The initial receive sequence number.
        uint32_t irs;

        // The sequence number of the last received package with data
        uint32_t dts;

        // expected next seq
        uint32_t expect;
    } rcv;

    // A timer used to time out the CLOSE-WAIT state.
    uint32_t closewait;

    struct {
        int time;
        bool flag;
    } wait_ack;

    struct {
        int time;
        bool flag;
    } wait_close;

    struct {
        int time;
        bool flag;
    } wait_keepalive;

    struct {
        int time;
        bool flag;
    } wait_keepalive_send;

    uint8_t *outbuf;
    uint8_t *recvbuf;
    size_t recvlen;
    size_t out_data_length;
    uint8_t local_port;
    uint8_t remote_port;
    struct rdp_cbs_s cbs;
    void *user_arg;
};

void rdp_init_connection(struct rdp_connection_s *conn, uint8_t *outbuf, uint8_t *recvbuf);
void rdp_reset_connection(struct rdp_connection_s *conn);

void rdp_set_send_cb(struct rdp_connection_s *conn, void (*send)(struct rdp_connection_s *, const uint8_t *, size_t));
void rdp_set_connected_cb(struct rdp_connection_s *conn, void (*connected)(struct rdp_connection_s *));
void rdp_set_closed_cb(struct rdp_connection_s *conn, void (*closed)(struct rdp_connection_s *));
void rdp_set_data_send_completed_cb(struct rdp_connection_s *conn, void (*data_send_completed)(struct rdp_connection_s *));
void rdp_set_data_received_cb(struct rdp_connection_s *conn, void (*data_received)(struct rdp_connection_s *, const uint8_t *, size_t));

void rdp_set_user_argument(struct rdp_connection_s *conn, void *user_arg);

bool rdp_listen(struct rdp_connection_s *conn, uint8_t port);
bool rdp_connect(struct rdp_connection_s *conn, uint8_t src_port, uint8_t dst_port);
bool rdp_close(struct rdp_connection_s *conn);

bool rdp_send(struct rdp_connection_s *conn, const uint8_t *data, size_t dlen);
bool rdp_can_send(struct rdp_connection_s *conn);

bool rdp_received(struct rdp_connection_s *conn, const uint8_t *inbuf);

void rdp_clock(struct rdp_connection_s *conn, int dt);
