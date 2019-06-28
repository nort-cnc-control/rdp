#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdio.h>
#include <rdp.h>
#include <rdpos.h>
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 

int fd_in, fd_out;

uint8_t received[RDP_MAX_SEGMENT_SIZE];
uint8_t inbuffer[RDP_MAX_SEGMENT_SIZE];
uint8_t outbuffer[RDP_MAX_SEGMENT_SIZE];

size_t lenrecv;

int cnctd = 0;

void send_serial(void *arg, const void *data, size_t dlen)
{
    struct rdpos_connection_s *conn = arg;
    printf("Sending %i bytes\n", dlen);
    printf("state = %i\n", conn->rdp_conn->state);
    write(fd_out, data, dlen);
}

void connected(struct rdp_connection_s *conn)
{
    printf("Connected\n");
    cnctd = 1;
}

void closed(struct rdp_connection_s *conn)
{
    printf("Connection closed\n");
}

void data_send_completed(struct rdp_connection_s *conn)
{
    printf("Data send completed\n");
}

void data_received(struct rdp_connection_s *conn, const uint8_t *buf, size_t len)
{
    printf("Received: %.*s\n", len, buf);
    lenrecv = len;
}

static void set_cbs(struct rdpos_connection_s *sconn, struct rdp_connection_s *conn)
{
    rdp_set_closed_cb(conn, closed);
    rdp_set_connected_cb(conn, connected);
    rdp_set_data_received_cb(conn, data_received);
    rdp_set_data_send_completed_cb(conn, data_send_completed);
    rdpos_set_send_cb(sconn, send_serial);
}

static uint8_t rdp_recv_buf[RDP_MAX_SEGMENT_SIZE];
static uint8_t rdp_outbuf[RDP_MAX_SEGMENT_SIZE];
static uint8_t serial_inbuf[RDP_MAX_SEGMENT_SIZE];

int main(void)
{
    fd_out = open("pipe_clt_srv", O_RDWR);
    fd_in = open("pipe_srv_clt", O_RDWR | O_NONBLOCK);

    struct rdpos_connection_s sconn;
    struct rdp_connection_s conn;

    rdp_init_connection(&conn, rdp_outbuf, rdp_recv_buf);
    rdpos_init_connection(&sconn, &conn, serial_inbuf, sizeof(serial_inbuf));
    set_cbs(&sconn, &conn);
    rdp_connect(&conn, 1, 1);

    while (true)
    {
        unsigned char b;
        usleep(1000);
        ssize_t n = read(fd_in, &b, 1);
        if (n < 1)
        {
            rdp_clock(&conn, 1000);
            continue;
        }

        if (rand() < RAND_MAX / 100)
	    {
	        printf("Data loss\n");
                continue;
	    }

        bool res = rdpos_byte_received(&sconn, b);
        /*if (lenrecv > 0)
        {
            rdp_close(conn);
            lenrecv = 0;
        }*/
        if (conn.state == RDP_CLOSED)
        {
            printf("Exit\n");
            break;
        }
    }

    return 0;
}
