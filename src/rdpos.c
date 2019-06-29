#include <stdio.h>
#include <rdpos.h>
#include <serial-datagram/serial_datagram.h>

static void rdp_send_hdl(struct rdp_connection_s *conn, const uint8_t *buf, size_t len)
{
    struct rdpos_connection_s *sconn = conn->user_arg;
    serial_datagram_send(buf, len, sconn->cbs.send_fn, sconn);
}

static void datagram_received(const void *data, size_t len, void *arg)
{
    struct rdpos_connection_s *sconn = arg;
    struct rdp_connection_s *conn = sconn->rdp_conn;
    bool res = rdp_received(conn, (const uint8_t*)data, len);
}

void rdpos_init_connection(struct rdpos_connection_s *conn,
                           struct rdp_connection_s *rconn,
                           uint8_t *serial_receive_buf,
                           size_t serial_receive_buf_len)
{
    conn->rdp_conn = rconn;

    // we do changge in place for memory save
    rdp_set_send_cb(rconn, rdp_send_hdl);
    rdp_set_user_argument(rconn, conn);

    serial_datagram_rcv_handler_init(&conn->serial_handler, serial_receive_buf, serial_receive_buf_len, datagram_received, conn);
}

void rdpos_set_send_cb(struct rdpos_connection_s *conn, void (*send)(void *, const void *, size_t))
{
    conn->cbs.send_fn = send;
}

bool rdpos_byte_received(struct rdpos_connection_s *conn, uint8_t data)
{
    int res = serial_datagram_receive(&conn->serial_handler, &data, 1);
    switch (res)
    {
        case SERIAL_DATAGRAM_RCV_NO_ERROR:
            return true;
        default:
            printf("ERR = %i\n", res);
            return false;
    }
}
