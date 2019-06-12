#include <cycle.h>
#include <packages.h>
#include <string.h>

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

static bool rdp_send_syn(struct rdp_connection_s *conn, uint8_t src_port, uint8_t dst_port)
{
    if (conn->state != RDP_CLOSED)
        return false;
    conn->local_port = src_port;
    conn->remote_port = dst_port;
    // send SYN
    conn->state = RDP_SYN_SENT;

    conn->snd.nxt = conn->snd.iss + 1;

    size_t len = rdp_build_syn_package(conn->outbuf, src_port, dst_port, conn->snd.nxt);
    conn->snd.una = conn->snd.nxt;
    conn->snd.nxt++;
    
    conn->out_data_length = len; 
    conn->cbs->send(conn, conn->outbuf, len);
    conn->cbs->ack_wait_start(conn, conn->snd.una);
    return true;
}

// Receie handlers
static bool rdp_syn_received(struct rdp_connection_s *conn, uint8_t src_port, uint8_t dst_port, uint32_t seq)
{
    if (conn->state == RDP_LISTEN)
    {
        if (conn->local_port != dst_port)
            return false;
        conn->remote_port = src_port;
    }
    if (conn->state == RDP_SYN_SENT || conn->state == RDP_LISTEN)
    {
        conn->state = RDP_SYN_RCVD;

        conn->rcv.irs = seq;
        conn->rcv.cur = seq;
        
        conn->snd.nxt = conn->snd.iss + 1;

        size_t len = rdp_build_synack_package(conn->outbuf, conn->local_port, conn->remote_port, conn->snd.nxt, conn->rcv.cur);
        conn->snd.una = conn->snd.nxt;
        conn->snd.nxt++;
        
        conn->out_data_length = len;
        conn->cbs->send(conn, conn->outbuf, len);
        conn->cbs->ack_wait_start(conn, conn->snd.una);
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
            conn->state = RDP_OPEN;
            conn->rcv.cur = seq;
            if (conn->snd.una == ack)
            {
                conn->snd.una = conn->snd.iss;
                conn->cbs->ack_wait_completed(conn, ack);
            }
            else if (conn->snd.una != conn->snd.iss)
            {
                return false;
            }
            size_t len = rdp_build_ack_package(conn->outbuf, conn->local_port, conn->remote_port, conn->snd.nxt, conn->rcv.cur, NULL, 0);

            conn->out_data_length = len;
            conn->cbs->send(conn, conn->outbuf, len);
            conn->cbs->connected(conn);
            return true;
        case RDP_SYN_RCVD:
            conn->state = RDP_OPEN;

            if (conn->snd.una == ack)
            {
                conn->snd.una = conn->snd.iss;
                conn->cbs->ack_wait_completed(conn, ack);
            }
            else if (conn->snd.una != conn->snd.iss)
            {
                return false;
            }

            conn->rcv.cur = seq;
            conn->cbs->connected(conn);
            return true;
        default:
            return false;
    }
    return false;
}

static bool rdp_rst_received(struct rdp_connection_s *conn, uint32_t seq)
{
    size_t len;
    switch(conn->state)
    {
        case RDP_OPEN:
            conn->rcv.cur = seq;
            conn->state = RDP_PASSIVE_CLOSE_WAIT;
            len = rdp_build_rstack_package(conn->outbuf, conn->local_port, conn->remote_port, conn->snd.nxt, conn->rcv.cur);
            conn->snd.una = conn->snd.nxt;
            conn->snd.nxt++;
            conn->out_data_length = len;
            conn->cbs->send(conn, conn->outbuf, len);
            conn->cbs->ack_wait_start(conn, conn->snd.una);
            conn->cbs->close_wait_start(conn);
            return true;
        case RDP_ACTIVE_CLOSE_WAIT:
            conn->state = RDP_CLOSED;
            conn->cbs->closed(conn);
            return true;
        case RDP_PASSIVE_CLOSE_WAIT:
            conn->cbs->ack_wait_completed(conn, conn->snd.una);
            len = rdp_build_rstack_package(conn->outbuf, conn->local_port, conn->remote_port, conn->snd.nxt, conn->rcv.cur);
            conn->snd.una = conn->snd.nxt;
            conn->snd.nxt++;
            conn->out_data_length = len;
            conn->cbs->send(conn, conn->outbuf, len);
            conn->cbs->ack_wait_start(conn, conn->snd.una);
            return true;
    }
    return false;
}

static bool rdp_rstack_received(struct rdp_connection_s *conn, uint32_t seq, uint32_t ack)
{
    if (conn->snd.una == ack)
    {
        conn->snd.una = conn->snd.iss;
        conn->cbs->ack_wait_completed(conn, ack);
    }
    else if (conn->snd.una != conn->snd.iss)
    {
        return false;
    }
    conn->rcv.cur = seq;
    if (conn->state == RDP_ACTIVE_CLOSE_WAIT)
    {
        conn->state = RDP_CLOSED;
        size_t len = rdp_build_ack_package(conn->outbuf, conn->local_port, conn->remote_port, conn->snd.nxt, conn->rcv.cur, NULL, 0);

        conn->out_data_length = len;
        conn->cbs->send(conn, conn->outbuf, len);
        conn->cbs->closed(conn);
        return true;
    }
    return false;
}

static bool rdp_nul_received(struct rdp_connection_s *conn, uint32_t seq)
{
    if (conn->state == RDP_OPEN)
    {
        conn->rcv.cur = seq;
        size_t len = rdp_build_ack_package(conn->outbuf, conn->local_port, conn->remote_port, conn->snd.nxt, conn->rcv.cur, NULL, 0);
        
        conn->out_data_length = len;
        conn->cbs->send(conn, conn->outbuf, len);
        return true;
    }
    return false;
}

static bool rdp_nulack_received(struct rdp_connection_s *conn, uint32_t seq, uint32_t ack)
{
    if (conn->state == RDP_OPEN)
    {
        conn->rcv.cur = seq;
        if (conn->snd.una == ack)
        {
            conn->snd.una = conn->snd.iss;
            conn->cbs->ack_wait_completed(conn, ack);
        }
        else if (conn->snd.una != conn->snd.iss)
        {
            return false;
        }
        
        size_t len = rdp_build_ack_package(conn->outbuf, conn->local_port, conn->remote_port, conn->snd.nxt, conn->rcv.cur, NULL, 0);
        
        conn->out_data_length = len;
        conn->cbs->send(conn, conn->outbuf, len);
        return true;
    }
    return false;
}

static bool rdp_empty_ack_received(struct rdp_connection_s *conn, uint32_t seq, uint32_t ack)
{
    if (conn->snd.una == ack)
    {
        conn->snd.una = conn->snd.iss;
        conn->cbs->ack_wait_completed(conn, ack);
    }
    else if (conn->snd.una != conn->snd.iss)
    {
        return false;
    }
    
    conn->rcv.cur = seq;
    switch(conn->state)
    {
        case RDP_SYN_RCVD:
            conn->state = RDP_OPEN;
            conn->cbs->connected(conn);
            return true;
        case RDP_OPEN:
            return true;
        case RDP_PASSIVE_CLOSE_WAIT:
            conn->state = RDP_CLOSED;
            conn->cbs->closed(conn);
            return true;
        default:
            return false;
    }
}

static bool rdp_ack_data_received(struct rdp_connection_s *conn, uint32_t seq, uint32_t ack, const uint8_t *data, size_t dlen)
{
    if (conn->snd.una == ack)
    {
        conn->snd.una = conn->snd.iss;
        conn->cbs->ack_wait_completed(conn, ack);
    }
    else if (conn->snd.una != conn->snd.iss)
    {
        return false;
    }
        

    conn->rcv.cur = seq;
    switch (conn->state)
    {
        case RDP_OPEN:
            if (seq > conn->rcv.dts)
            {
                memcpy(conn->recvbuf, data, dlen);
                conn->cbs->data_received(conn, conn->recvbuf, dlen);
            }
            conn->rcv.dts = seq;
            size_t len = rdp_build_ack_package(conn->outbuf, conn->local_port, conn->remote_port, conn->snd.nxt, conn->rcv.cur, NULL, 0);
            conn->out_data_length = len;
            conn->cbs->send(conn, conn->outbuf, len);
            return true;
        default:
            return false;
    }
}

static bool rdp_ack_received(struct rdp_connection_s *conn, const uint8_t *inbuf)
{
    struct rdp_header_s *hdr = (struct rdp_header_s *)inbuf;

    size_t pdlen = hdr->data_length;
    if (pdlen > 0)
    {
        const uint8_t *data = inbuf + hdr->header_length * 2;
        return rdp_ack_data_received(conn, hdr->sequence_number, hdr->acknowledgement_number, data, pdlen);
    }
    else
    {
        return rdp_empty_ack_received(conn, hdr->sequence_number, hdr->acknowledgement_number);
    }
}

// Connection operations

void rdp_init_connection(struct rdp_connection_s *conn,
                         uint8_t *outbuf, uint8_t *recvbuf,
                         struct rdp_cbs_s *cbs)
{
    rdp_reset_connection(conn);
    conn->snd.iss = 1;
    conn->outbuf = outbuf;
    conn->recvbuf = recvbuf;
    conn->recvlen = 0;
    conn->cbs = cbs;
}

void rdp_reset_connection(struct rdp_connection_s *conn)
{
    memset(&conn->snd, 0, sizeof(conn->snd));
    memset(&conn->rcv, 0, sizeof(conn->rcv));
    memset(&conn->seg, 0, sizeof(conn->seg));
    conn->state = RDP_CLOSED;
}

bool rdp_listen(struct rdp_connection_s *conn, uint8_t port)
{
    if (conn->state != RDP_CLOSED)
        return false;
    conn->snd.nxt = conn->snd.iss + 1;
    conn->snd.una = conn->snd.iss;
    conn->local_port = port;
    conn->state = RDP_LISTEN;
    return true;
}

bool rdp_connect(struct rdp_connection_s *conn, uint8_t src_port, uint8_t dst_port)
{
    return rdp_send_syn(conn, src_port, dst_port);
}

bool rdp_close(struct rdp_connection_s *conn)
{
    if (conn->state != RDP_OPEN)
        return false;
    conn->state = RDP_ACTIVE_CLOSE_WAIT;
    size_t len = rdb_build_rst_package(conn->outbuf, conn->local_port, conn->remote_port, conn->snd.nxt, conn->rcv.cur);
    conn->snd.una = conn->snd.nxt;
    conn->snd.nxt++;
    conn->out_data_length = len;
    conn->cbs->send(conn, conn->outbuf, len);
    conn->cbs->ack_wait_start(conn, conn->snd.una);
    conn->cbs->close_wait_start(conn);
    return true;
}

bool rdp_final_close(struct rdp_connection_s *conn)
{
    if (conn->state == RDP_CLOSED)
        return true;
    if (conn->state != RDP_ACTIVE_CLOSE_WAIT &&
        conn->state != RDP_PASSIVE_CLOSE_WAIT)
        return false;
    conn->state = RDP_CLOSED;
    conn->cbs->closed(conn);
    return true;
}

bool rdp_send(struct rdp_connection_s *conn, const uint8_t *data, size_t dlen)
{
    if (conn->state != RDP_OPEN)
        return false;
    if (!rdp_can_send(conn))
        return false;
    // Actual data sending
    conn->snd.dts = conn->snd.nxt;
    size_t len = rdp_build_ack_package(conn->outbuf, conn->local_port, conn->remote_port, conn->snd.nxt, conn->rcv.cur, data, dlen);
    conn->snd.una = conn->snd.nxt;
    conn->snd.nxt++;
    conn->out_data_length = len;
    conn->cbs->send(conn, conn->outbuf, len);
    conn->cbs->ack_wait_start(conn, conn->snd.una);
    return true;
}

bool rdp_received(struct rdp_connection_s *conn, const uint8_t *inbuf)
{
    struct rdp_header_s *hdr = (struct rdp_header_s *)inbuf;

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
    conn->cbs->send(conn, conn->outbuf, conn->out_data_length);
    return true;
}

bool rdp_can_send(struct rdp_connection_s *conn)
{
    if (conn->state != RDP_OPEN)
        return false;
    return (conn->snd.una == conn->snd.iss);
}
