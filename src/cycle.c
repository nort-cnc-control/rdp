#include <cycle.h>
#include <packages.h>
#include <string.h>
#include <stdio.h>

/*

Based on https://tools.ietf.org/html/rfc908

With modifications:

1. "rcv ACK or rcv SYN,ACK" instead of "rcv ACK" in SYN-RCVD state
2. Connection finishing changed

                             +------------+
              Passive Open   |            |<------------------------------+
            +----------------|   CLOSED   |                               |
            |   Request      |            |---------------+               |
            V                +------------+               |               |
     +------------+                                       |               |
     |            |                                       |               |
     |   LISTEN   |                                       |               |
     |            |                                       |               |
     +------------+                                       |               |
            |                                   Active    |               |
            |  rcv SYN                       Open Request |               |
            | -----------                    ------------ |               |
            | snd SYN,ACK                      snd SYN    |               |
            V                   rcv SYN                   V               |
     +------------+          -----------           +------------+         |
     |            |          snd SYN,ACK           |            |         |
     |  SYN-RCVD  |<-------------------------------|  SYN-SENT  |         |
     |            |                                |            |         |
     +------------+                                +------------+         |
            |  rcv ACK or rcv SYN,ACK        rcv SYN,ACK  |               |
            | ----------                    ------------- |               |
            |    xxx         +------------+    snd ACK    |               |
            |                |            |               |               |
            +--------------->|    OPEN    |<--------------+               |
                             |            |                               |
                             +------------+                               |
            rcv RST         /              \   Close request              |
        --------------     /                \ ---------------             |
        send RST,ACK      /                  \    snd RST                 |
                         V                    V                           |
                  +-------------+       +------------+                    |
        +---------|             |       |            |                    |
rcv RST |         |  PASSIVE    |       |   ACTIVE   |   rcv RST,ACK      |
------- |         | CLOSE WAIT  |       | CLOSE-WAIT |   ----------       |
RST,ACK |         |             |       |            |     snd ACK        |
        +-------->|             |       |            |------------------->|
                  +-------------+       |            |                    |
                    |       |           |            |                    |
                    |       |           |            |    rcv RST         |
            rcv ACK |       |           |            |   ---------        |
           ---------|       |           |            |      xxx           |
              xxx   |       |           |            |------------------->|
                    |       |           +------------+                    |
                    |       | delay           |                           |
                    |       |                 | delay                     |
                    |       V                 V                           |
                    +-----------------------------------------------------+
                                   


                       RDP Connection State Diagram
                                 Figure 3


*/

static void rdp_pkg_rcvd(struct rdp_connection_s *conn)
{
    conn->wait_keepalive.time = 0;
}

static bool rdp_send_nul(struct rdp_connection_s *conn)
{
    if (conn->state != RDP_OPEN)
        return false;
    if (!rdp_can_send(conn))
        return false;

    size_t len = rdp_build_nul_package(conn->outbuf, conn->local_port, conn->remote_port, conn->snd.nxt, conn->rcv.cur);
    conn->snd.una = conn->snd.nxt;
    //printf("SEND CLOCK. Set una = %i\n", conn->snd.una);
    if (conn->cbs.send)
        conn->cbs.send(conn, conn->outbuf, len);
    conn->wait_ack.time = 0;
    conn->wait_ack.flag = 1;
    conn->wait_keepalive_send.time = 0;
    return true;
}

static bool rdp_send_syn(struct rdp_connection_s *conn, uint8_t src_port, uint8_t dst_port)
{
    if (conn->state != RDP_CLOSED)
        return false;
    conn->local_port = src_port;
    conn->remote_port = dst_port;
    // send SYN
    conn->state = RDP_SYN_SENT;

    conn->snd.nxt = conn->snd.iss + 1;
    conn->snd.dts = conn->snd.iss;

    size_t len = rdp_build_syn_package(conn->outbuf, src_port, dst_port, conn->snd.nxt);
    conn->snd.una = conn->snd.nxt;
    conn->snd.nxt++;
    
    conn->out_data_length = len;
    if (conn->cbs.send)
        conn->cbs.send(conn, conn->outbuf, len);
    conn->wait_ack.time = 0;
    conn->wait_ack.flag = 1;
    return true;
}

// Receie handlers
static bool rdp_syn_received(struct rdp_connection_s *conn, uint8_t src_port, uint8_t dst_port, uint32_t seq)
{
    conn->wait_keepalive.time = 0;
    conn->wait_keepalive.flag = 1;
    if (conn->state == RDP_LISTEN)
    {
        if (conn->local_port != dst_port)
            return false;
        conn->remote_port = src_port;
    }
    if (conn->state == RDP_SYN_SENT || conn->state == RDP_LISTEN || conn->state == RDP_SYN_RCVD)
    {
        conn->state = RDP_SYN_RCVD;

        conn->rcv.dts = seq;
        conn->rcv.irs = seq;
        conn->rcv.cur = seq;
        conn->rcv.expect = seq + 1;
        
        conn->snd.dts = conn->snd.iss;
        conn->snd.nxt = conn->snd.iss + 1;

        size_t len = rdp_build_synack_package(conn->outbuf, conn->local_port, conn->remote_port, conn->snd.nxt, conn->rcv.cur);
        conn->snd.una = conn->snd.nxt;
        conn->snd.nxt++;
        
        conn->out_data_length = len;
        if (conn->cbs.send)
            conn->cbs.send(conn, conn->outbuf, len);
        conn->wait_ack.time = 0;
        conn->wait_ack.flag = 1;
        return true;
    }
    return false;
}

static bool rdp_synack_received(struct rdp_connection_s *conn, uint32_t seq, uint32_t ack)
{
    switch (conn->state)
    {
        case RDP_OPEN:
        case RDP_SYN_SENT:
            if (conn->snd.una == ack)
            {
                conn->snd.una = conn->snd.iss;
                conn->wait_ack.flag = 0;
            }
            else if (conn->snd.una != conn->snd.iss)
            {
                return false;
            }

            conn->rcv.cur = seq;
            conn->rcv.expect = seq + 1;

            size_t len = rdp_build_ack_package(conn->outbuf, conn->local_port, conn->remote_port, conn->snd.nxt, conn->rcv.cur, NULL, 0);

            conn->out_data_length = len;
            if (conn->cbs.send)
                conn->cbs.send(conn, conn->outbuf, len);
            if (conn->state != RDP_OPEN)
            {
                conn->state = RDP_OPEN;
                conn->wait_keepalive_send.time = 0;
                conn->wait_keepalive_send.flag = 1;
                if (conn->cbs.connected)
                    conn->cbs.connected(conn);
            }
            return true;
        case RDP_SYN_RCVD:
            if (conn->snd.una == ack)
            {
                conn->wait_ack.flag = 0;
                conn->snd.una = conn->snd.iss;
            }
            else if (conn->snd.una != conn->snd.iss)
            {
                return false;
            }
            conn->state = RDP_OPEN;
            conn->rcv.cur = seq;
            if (conn->cbs.connected)
                conn->cbs.connected(conn);
            return true;
        default:
            return false;
    }
    return false;
}

static bool rdp_rst_received(struct rdp_connection_s *conn, uint32_t seq)
{
    size_t len;
    if (conn->rcv.expect != seq && conn->rcv.cur != seq)
    {
        return false;
    }
    conn->wait_keepalive_send.flag = 0;
    switch(conn->state)
    {
        case RDP_OPEN:
            conn->rcv.cur = seq;
            conn->rcv.expect = seq + 1;
            conn->state = RDP_PASSIVE_CLOSE_WAIT;
            len = rdp_build_rstack_package(conn->outbuf, conn->local_port, conn->remote_port, conn->snd.nxt, conn->rcv.cur);
            conn->snd.una = conn->snd.nxt;
            conn->snd.nxt++;
            conn->out_data_length = len;
            if (conn->cbs.send)
                conn->cbs.send(conn, conn->outbuf, len);
            conn->wait_ack.time = 0;
            conn->wait_ack.flag = 1;
            conn->wait_close.time = 0;
            conn->wait_close.flag = 1;
            conn->wait_keepalive_send.flag = 0;
            return true;
        case RDP_ACTIVE_CLOSE_WAIT:
            conn->state = RDP_CLOSED;
            if (conn->cbs.closed)
                conn->cbs.closed(conn);
            return true;
        case RDP_PASSIVE_CLOSE_WAIT:
            conn->rcv.cur = seq;
            conn->rcv.expect = seq + 1;
            conn->wait_ack.flag = 0;
            len = rdp_build_rstack_package(conn->outbuf, conn->local_port, conn->remote_port, conn->snd.nxt, conn->rcv.cur);
            conn->snd.una = conn->snd.nxt;
            conn->snd.nxt++;
            conn->out_data_length = len;
            if (conn->cbs.send)
                conn->cbs.send(conn, conn->outbuf, len);
            conn->wait_ack.time = 0;
            conn->wait_ack.flag = 1;
            conn->wait_close.time = 0;
            conn->wait_close.flag = 1;
            conn->wait_keepalive_send.flag = 0;
            return true;
    }
    return false;
}

static bool rdp_rstack_received(struct rdp_connection_s *conn, uint32_t seq, uint32_t ack)
{
    if (conn->rcv.expect != seq && conn->rcv.cur != seq)
    {
        return false;
    }
    if (conn->snd.una == ack)
    {
        conn->wait_ack.flag = 0;
        conn->snd.una = conn->snd.iss;
    }
    else if (conn->snd.una != conn->snd.iss)
    {
        return false;
    }
    conn->rcv.cur = seq;
    conn->rcv.expect = seq + 1;
    if (conn->state == RDP_ACTIVE_CLOSE_WAIT)
    {
        conn->state = RDP_CLOSED;
        size_t len = rdp_build_ack_package(conn->outbuf, conn->local_port, conn->remote_port, conn->snd.nxt, conn->rcv.cur, NULL, 0);

        conn->out_data_length = len;
        if (conn->cbs.send)
            conn->cbs.send(conn, conn->outbuf, len);
        if (conn->cbs.closed)
            conn->cbs.closed(conn);
        return true;
    }
    return false;
}

static bool rdp_nul_received(struct rdp_connection_s *conn, uint32_t seq)
{
    if (conn->state == RDP_OPEN)
    {
        if (conn->rcv.expect != seq && conn->rcv.cur != seq)
        {
            return false;
        }
        conn->rcv.cur = seq;
        conn->rcv.expect = seq;
        size_t len = rdp_build_ack_package(conn->outbuf, conn->local_port, conn->remote_port, conn->snd.nxt, conn->rcv.cur, NULL, 0);
        
        conn->out_data_length = len;
        if (conn->cbs.send)
            conn->cbs.send(conn, conn->outbuf, len);
        return true;
    }
    return false;
}

static bool rdp_empty_ack_received(struct rdp_connection_s *conn, uint32_t seq, uint32_t ack)
{
    conn->rcv.expect = seq;
    switch(conn->state)
    {
        case RDP_SYN_RCVD:
            conn->state = RDP_OPEN;
            conn->wait_keepalive_send.time = 0;
            conn->wait_keepalive_send.flag = 1;
            if (conn->cbs.connected)
                conn->cbs.connected(conn);
            return true;
        case RDP_OPEN:
            return true;
        case RDP_PASSIVE_CLOSE_WAIT:
            conn->state = RDP_CLOSED;
            if (conn->cbs.closed)
                conn->cbs.closed(conn);
            return true;
        default:
            return false;
    }
}

static bool rdp_ack_data_received(struct rdp_connection_s *conn, uint32_t seq, uint32_t ack, const uint8_t *data, size_t dlen, bool *rcvd)
{
    conn->rcv.expect = seq + 1;
    switch (conn->state)
    {
        case RDP_OPEN:
            if (seq > conn->rcv.dts)
            {
                memcpy(conn->recvbuf, data, dlen);
                *rcvd = true;
            }
            size_t len = rdp_build_ack_package(conn->outbuf, conn->local_port, conn->remote_port, conn->snd.nxt, conn->rcv.cur, NULL, 0);
            conn->out_data_length = len;
            if (conn->cbs.send)
                conn->cbs.send(conn, conn->outbuf, len);
            return true;
        case RDP_PASSIVE_CLOSE_WAIT:
            conn->state = RDP_CLOSED;
            if (conn->cbs.closed)
                conn->cbs.closed(conn);
            return true;
        default:
            return false;
    }
}

static bool rdp_ack_received(struct rdp_connection_s *conn, const uint8_t *inbuf)
{
    struct rdp_header_s *hdr = (struct rdp_header_s *)inbuf;
    
    uint32_t seq = hdr->sequence_number;
    uint32_t ack = hdr->acknowledgement_number;

    //printf("ACK received. seq = %i, ack = %i, expect = %i, cur = %i, dts = %i, una = %i\n", seq, ack, conn->rcv.expect, conn->rcv.cur, conn->snd.dts, conn->snd.una);

    if (conn->rcv.expect != seq && conn->rcv.cur != seq)
    {
        return false;
    }
    if (conn->snd.una == conn->snd.iss)
    {
        // ok
    }
    else if (ack == conn->snd.una)
    {
        conn->wait_ack.flag = 0;
        conn->snd.una = conn->snd.iss;
    }
    else if (ack < conn->snd.una)
    {
        // ok
    }
    else
    {
        return false;
    }
    
    conn->rcv.cur = seq;
    
    size_t pdlen = hdr->data_length;
    bool res = false;
    bool rcvd = false;
    if (pdlen > 0)
    {
        const uint8_t *data = inbuf + hdr->header_length * 2;
        res = rdp_ack_data_received(conn, seq, ack, data, pdlen, &rcvd);
    }
    else
    {
        res = rdp_empty_ack_received(conn, seq, ack);
    }

    if (conn->snd.dts != conn->snd.iss && ack == conn->snd.dts)
    {
        //printf("COMPLETED\n");
        conn->snd.dts = conn->snd.iss;
        if (conn->cbs.data_send_completed)
            conn->cbs.data_send_completed(conn);
    }
    else
    {
        //printf("seq = %i, ack = %i, dts = %i\n", seq, ack, conn->snd.dts);
    }
    
    if (rcvd)
    {
        conn->rcv.dts = seq;
        if (conn->cbs.data_received)
            conn->cbs.data_received(conn, conn->recvbuf, pdlen);
    }
    return res;
}

// Initialization

void rdp_init_connection(struct rdp_connection_s *conn, uint8_t *outbuf, uint8_t *recvbuf)
{
    memset(conn, 0, sizeof(*conn));
    rdp_reset_connection(conn);
    conn->outbuf = outbuf;
    conn->recvbuf = recvbuf;
    conn->snd.iss = 1;
}

void rdp_set_send_cb(struct rdp_connection_s *conn, void (*send)(struct rdp_connection_s *, const uint8_t *, size_t))
{
    conn->cbs.send = send;
}

void rdp_set_connected_cb(struct rdp_connection_s *conn, void (*connected)(struct rdp_connection_s *))
{
    conn->cbs.connected = connected;
}

void rdp_set_closed_cb(struct rdp_connection_s *conn, void (*closed)(struct rdp_connection_s *))
{
    conn->cbs.closed = closed;
}

void rdp_set_data_send_completed_cb(struct rdp_connection_s *conn, void (*data_send_completed)(struct rdp_connection_s *))
{
    conn->cbs.data_send_completed = data_send_completed;
}

void rdp_set_data_received_cb(struct rdp_connection_s *conn, void (*data_received)(struct rdp_connection_s *, const uint8_t *, size_t))
{
    conn->cbs.data_received = data_received;
}

void rdp_set_user_argument(struct rdp_connection_s *conn, void *user_arg)
{
    conn->user_arg = user_arg;
}

// Connection operations


void rdp_reset_connection(struct rdp_connection_s *conn)
{
    memset(&conn->snd, 0, sizeof(conn->snd));
    memset(&conn->rcv, 0, sizeof(conn->rcv));
    conn->wait_ack.flag = 0;
    conn->wait_close.flag = 0;
    conn->wait_keepalive.flag = 0;
    conn->wait_keepalive_send.flag = 0;
    conn->state = RDP_CLOSED;
}

bool rdp_listen(struct rdp_connection_s *conn, uint8_t port)
{
    if (conn->state != RDP_CLOSED)
        return false;
    conn->snd.dts = conn->snd.iss;
    conn->snd.nxt = conn->snd.iss + 1;
    conn->snd.una = conn->snd.iss;
    conn->local_port = port;
    conn->state = RDP_LISTEN;
    return true;
}

bool rdp_connect(struct rdp_connection_s *conn, uint8_t src_port, uint8_t dst_port)
{
    conn->wait_keepalive.time = 0;
    conn->wait_keepalive.flag = 1;
    return rdp_send_syn(conn, src_port, dst_port);
}

bool rdp_close(struct rdp_connection_s *conn)
{
    //printf("*** closing. state=%i\n", conn->state);
    switch (conn->state)
    {
    case RDP_OPEN: {
        conn->state = RDP_ACTIVE_CLOSE_WAIT;
        size_t len = rdb_build_rst_package(conn->outbuf, conn->local_port, conn->remote_port, conn->snd.nxt, conn->rcv.cur);
        conn->snd.una = conn->snd.nxt;
        conn->snd.nxt++;
        conn->out_data_length = len;
        if (conn->cbs.send)
            conn->cbs.send(conn, conn->outbuf, len);
        conn->wait_ack.time = 0;
        conn->wait_ack.flag = 1;
        conn->wait_close.time = 0;
        conn->wait_close.flag = 1;
        conn->wait_keepalive_send.flag = 0;
        return true;
    }
    case RDP_LISTEN: {
        conn->state = RDP_CLOSED;
        return true;
    }
    default:
        return false;
    }
}

bool rdp_final_close(struct rdp_connection_s *conn)
{
    //printf("FINAL CLOSE\n");
    if (conn->state == RDP_CLOSED)
        return true;
    if (conn->state != RDP_ACTIVE_CLOSE_WAIT &&
        conn->state != RDP_PASSIVE_CLOSE_WAIT)
        return false;
    conn->wait_keepalive.flag = 0;
    conn->wait_ack.flag = 0;
    conn->wait_close.flag = 0;
    conn->wait_keepalive_send.flag = 0;
    conn->state = RDP_CLOSED;
    if (conn->cbs.closed)
        conn->cbs.closed(conn);
    return true;
}

bool rdp_send(struct rdp_connection_s *conn, const uint8_t *data, size_t dlen)
{
    if (conn->state != RDP_OPEN)
        return false;
    if (!rdp_can_send(conn))
    {
        return false;
    }
    // Actual data sending
    size_t len = rdp_build_ack_package(conn->outbuf, conn->local_port, conn->remote_port, conn->snd.nxt, conn->rcv.cur, data, dlen);
    conn->snd.una = conn->snd.nxt;
    conn->snd.dts = conn->snd.nxt;
    conn->snd.nxt++;
    conn->out_data_length = len;
    if (conn->cbs.send)
        conn->cbs.send(conn, conn->outbuf, len);
    conn->wait_ack.time = 0;
    conn->wait_ack.flag = 1;
    conn->wait_keepalive_send.time = 0;
    //printf("SEND. dts = %i\n", conn->snd.dts);
    return true;
}

bool rdp_received(struct rdp_connection_s *conn, const uint8_t *inbuf, size_t len)
{
    if (len < sizeof(struct rdp_header_s))
        return false;
    struct rdp_header_s *hdr = (struct rdp_header_s *)inbuf;
    if (len < hdr->header_length + hdr->data_length)
        return false;
    rdp_pkg_rcvd(conn);
    enum rdp_package_type_e type = rdp_package_type(inbuf);
    switch (type)
    {
        case RDP_SYN:
            if (hdr->destination_port != conn->local_port)
                return false;
            return rdp_syn_received(conn, hdr->source_port, hdr->destination_port, hdr->sequence_number);
        case RDP_ACK:
            if (hdr->source_port != conn->remote_port || hdr->destination_port != conn->local_port)
                return false;
            return rdp_ack_received(conn, inbuf);
        case RDP_SYNACK:
            if (hdr->source_port != conn->remote_port || hdr->destination_port != conn->local_port)
                return false;
            return rdp_synack_received(conn, hdr->sequence_number, hdr->acknowledgement_number);
        case RDP_NUL:
            if (hdr->source_port != conn->remote_port || hdr->destination_port != conn->local_port)
                return false;
            return rdp_nul_received(conn, hdr->sequence_number);
        case RDP_RST:
            if (hdr->source_port != conn->remote_port || hdr->destination_port != conn->local_port)
                return false;
            return rdp_rst_received(conn, hdr->sequence_number);
        case RDP_RSTACK:
            if (hdr->source_port != conn->remote_port || hdr->destination_port != conn->local_port)
                return false;
            return rdp_rstack_received(conn, hdr->sequence_number, hdr->acknowledgement_number);
        case RDP_EACK:
            if (hdr->source_port != conn->remote_port || hdr->destination_port != conn->local_port)
                return false;
            // Not supported
            break;
        default:
            break;
    }
    
    return false;
}

bool rdp_retry(struct rdp_connection_s *conn)
{
    if (conn->cbs.send)
        conn->cbs.send(conn, conn->outbuf, conn->out_data_length);
    return true;
}

bool rdp_can_send(struct rdp_connection_s *conn)
{
    if (conn->state != RDP_OPEN)
        return false;
    //printf("CHECK. una = %i, iss = %i\n", conn->snd.una, conn->snd.iss);
    return (conn->snd.una == conn->snd.iss);
}

void rdp_clock(struct rdp_connection_s *conn, int dt)
{
    if (conn->wait_ack.flag)
    {
        conn->wait_ack.time += dt;
        if (conn->wait_ack.time > RDP_RESEND_TIMEOUT)
        {
            conn->wait_ack.time = 0;
            rdp_retry(conn);
        }
    }
    if (conn->wait_close.flag)
    {
        //printf("****** Waiting for close\n");
        conn->wait_close.time += dt;
        if (conn->wait_close.time > RDP_CLOSE_TIMEOUT)
        {
            conn->wait_close.time = 0;
            rdp_final_close(conn);
        }
    }
    if (conn->wait_keepalive.flag)
    {
        conn->wait_keepalive.time += dt;
        if (conn->wait_keepalive.time > RDP_KEEPALIVE_TIMEOUT)
        {
            conn->wait_keepalive.time = 0;
            conn->wait_keepalive.flag = 0;
            rdp_close(conn);
        }
    }
    if (conn->wait_keepalive_send.flag)
    {
        conn->wait_keepalive_send.time += dt;
        if (conn->wait_keepalive_send.time > RDP_KEEPALIVE_SEND_TIMEOUT)
        {
            rdp_send_nul(conn);
        }
    }
}
