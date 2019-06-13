#include <rdpos.h>
#include <serial-datagram/serial_datagram.h>

static void rdp_send_hdl(struct rdp_connection_s *conn, const uint8_t *buf, size_t len)
{
    struct rdpos_connection_s *sconn = conn->user_arg;
    sconn->cbs->send_fn(sconn, buf, len);
}

static void datagram_received(const void *data, size_t len, void *arg)
{
    struct rdpos_connection_s *sconn = arg;
    struct rdp_connection_s *conn = &sconn->rdp_conn;

    bool res = rdp_received(conn, (const uint8_t*)data);
}

void rdpos_init_connection(struct rdpos_connection_s *conn,
                           uint8_t *rdp_outbuf,
                           size_t rdp_outbuf_len,

                           uint8_t *rdp_recvbuf,
                           size_t rdp_recvbuf_len,

                           uint8_t *serial_receive_buf,
                           size_t serial_receive_buf_len,

                           struct rdp_cbs_s *rdp_cbs,
                           struct rdpos_cbs_s *rdpos_cbs)
{
    // we do changge in place for memory save
    rdp_cbs->send = rdp_send_hdl;

    rdp_init_connection(&conn->rdp_conn, rdp_outbuf, rdp_recvbuf, rdp_cbs, conn);

    conn->cbs = rdpos_cbs;

    serial_datagram_rcv_handler_init(&conn->serial_handler, serial_receive_buf, serial_receive_buf_len, datagram_received, conn);
}

bool rdpos_byte_received(struct rdpos_connection_s *conn, uint8_t data)
{
    serial_datagram_receive(&conn->serial_handler, &data, 1);
}
