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
#include <sys/stat.h>

int fd_in, fd_out;

uint8_t received[RDP_MAX_SEGMENT_SIZE];
size_t lenrecv;
uint8_t inbuffer[RDP_MAX_SEGMENT_SIZE];
uint8_t outbuffer[RDP_MAX_SEGMENT_SIZE];

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
    cnctd = 0;
}

static int ind = 0;
static const char *msgs[] = {
    "hello",
    "world",
    "baby",
    NULL
};

void data_send_completed(struct rdp_connection_s *conn)
{
    if (msgs[ind] == NULL)
    {
        printf("Data send completed. Finish\n");
        rdp_close(conn);
    }
    else
    {
        printf("Data send completed. Send next: %s\n", msgs[ind]);
        rdp_send(conn, msgs[ind], strlen(msgs[ind]));
        ind++;
    }
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

struct rdpos_connection_s sconn;
struct rdp_connection_s conn;

void send_msgs(void)
{
    rdp_send(&conn, msgs[0], strlen(msgs[0]));
    ind = 1;
}

int main(void)
{
    mkfifo("pipe_srv_clt", 0777);
    mkfifo("pipe_clt_srv", 0777);

    fd_out = open("pipe_srv_clt", O_RDWR);
    fd_in = open("pipe_clt_srv", O_RDWR | O_NONBLOCK);

    rdp_init_connection(&conn, rdp_outbuf, rdp_recv_buf);
    rdpos_init_connection(&sconn, &conn, serial_inbuf, sizeof(serial_inbuf));
    set_cbs(&sconn, &conn);
    rdp_listen(&conn, 1);

    printf("state = %i\n", conn.state);

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

	    bool res = rdpos_byte_received(&sconn, b);
        if (cnctd)
        {
            printf("Connected: state = %i\n", conn.state);
            cnctd = 0;
            send_msgs();
        }

        if (conn.state == RDP_CLOSED)
        {
            printf("Closed: state = %i\n", conn.state);
            rdp_listen(&conn, 1);
            printf("Listening: state = %i\n", conn.state); 
        }
    }

    printf("Hello message sent.\n");  

    return 0;
}
