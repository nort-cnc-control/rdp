#include <stdio.h>
#include <rdpos.h>
#include <serial-datagram/serial_datagram.h>

static void rdp_send_hdl(struct rdp_connection_s *conn, const uint8_t *buf, size_t len)
{
    struct rdpos_connection_s *sconn = conn->user_arg;
    serial_datagram_send(buf, len, sconn->cbs->send_fn, sconn);
}

static void datagram_received(const void *data, size_t len, void *arg)
{
    struct rdpos_connection_s *sconn = arg;
    struct rdp_connection_s *conn = &sconn->rdp_conn;
/*
    int i;
    printf("\nDATAGRAM RCV: (%i) ", len);
    for (i = 0; i < len; i++)
        printf("%02X ", ((const uint8_t*)data)[i]);
    printf("\n");*/
    bool res = rdp_received(conn, (const uint8_t*)data);
/*
    printf("\nRES = %i\n", res);
*/
}

void rdpos_init_connection(struct rdpos_connection_s *conn,
                           struct rdpos_buffer_set_s *bufs,
                           struct rdp_cbs_s *rdp_cbs,
                           struct rdpos_cbs_s *rdpos_cbs)
{
    // we do changge in place for memory save
    rdp_cbs->send = rdp_send_hdl;

    rdp_init_connection(&conn->rdp_conn, bufs->rdp_outbuf, bufs->rdp_recvbuf, rdp_cbs, conn);

    conn->cbs = rdpos_cbs;

    serial_datagram_rcv_handler_init(&conn->serial_handler, bufs->serial_receive_buf, bufs->serial_receive_buf_len, datagram_received, conn);
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
