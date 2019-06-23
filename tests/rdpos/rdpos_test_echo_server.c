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
    printf("state = %i\n", conn->rdp_conn.state);
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

void data_send_completed(struct rdp_connection_s *conn)
{
    printf("Data send completed\n");
}

void data_received(struct rdp_connection_s *conn, const uint8_t *buf, size_t len)
{
    printf("Received: %.*s\n", len, buf);
    lenrecv = len;
}

static struct rdp_cbs_s cbs = {
    .connected = connected,
    .closed = closed,
    .data_send_completed = data_send_completed,
    .data_received = data_received,
};

static struct rdpos_cbs_s scbs = {
    .send_fn = send_serial,
};

static uint8_t rdp_recv_buf[RDP_MAX_SEGMENT_SIZE];
static uint8_t rdp_outbuf[RDP_MAX_SEGMENT_SIZE];
static uint8_t serial_inbuf[RDP_MAX_SEGMENT_SIZE];

static struct rdpos_buffer_set_s bufs = {
    .rdp_outbuf = rdp_outbuf,
    .rdp_outbuf_len = RDP_MAX_SEGMENT_SIZE,
    .rdp_recvbuf = rdp_recv_buf,
    .rdp_recvbuf_len = RDP_MAX_SEGMENT_SIZE,
    .serial_receive_buf = serial_inbuf,
    .serial_receive_buf_len = RDP_MAX_SEGMENT_SIZE,
};

int main(void)
{
    mkfifo("pipe_srv_clt", 0777);
    mkfifo("pipe_clt_srv", 0777);

    fd_out = open("pipe_srv_clt", O_RDWR);
    fd_in = open("pipe_clt_srv", O_RDWR | O_NONBLOCK);

    struct rdpos_connection_s sconn;
    struct rdp_connection_s *conn = &sconn.rdp_conn;

    rdpos_init_connection(&sconn, &bufs, &cbs, &scbs);
    rdp_listen(conn, 1);

    printf("state = %i\n", conn->state);

    while (true)
    {
        unsigned char b;
        usleep(1000);
        ssize_t n = read(fd_in, &b, 1);
        if (n < 1)
        {
            rdp_clock(conn, 1000);
            continue;
        }
/*
        if (rand() < RAND_MAX / 100)
	    {
	        printf("Data loss\n");
                continue;
	    }*/
	    bool res = rdpos_byte_received(&sconn, b);
        if (cnctd)
        {
            printf("Connected: state = %i\n", conn->state);
            cnctd = 0;
            const char hello[] = "Hello, world!";
            rdp_send(conn, hello, sizeof(hello) - 1);
        }

        if (conn->state == RDP_CLOSED)
        {
            printf("Closed: state = %i\n", conn->state);
            rdp_listen(conn, 1);
            printf("Listening: state = %i\n", conn->state); 
        }
    }

    printf("Hello message sent.\n");  

    return 0;
}
