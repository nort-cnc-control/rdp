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

struct timeval tvr1, tvr2, dtvr;
struct timeval tvc1, tvc2, dtvc;
struct timezone tz;
bool waitack = false;
bool waitclose = false;

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
    waitack = false;
    waitclose = false;
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

void ack_wait_start(struct rdp_connection_s *conn, uint32_t seq)
{
    printf("Waiting ack %i start\n", seq);
    waitack = true;
    gettimeofday(&tvr1, &tz);
}

void ack_wait_completed(struct rdp_connection_s *conn, uint32_t seq)
{
    waitack = false;
    printf("Waiting ack %i completed\n", seq);
}

void close_wait_start(struct rdp_connection_s *conn)
{
    waitclose = true;
    printf("Waiting for connection close\n");
    gettimeofday(&tvc1, &tz);
}

static struct rdp_cbs_s cbs = {
    .connected = connected,
    .closed = closed,
    .data_send_completed = data_send_completed,
    .data_received = data_received,
    .ack_wait_start = ack_wait_start,
    .ack_wait_completed = ack_wait_completed,
    .close_wait_start = close_wait_start,
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
        usleep(10000);
        ssize_t n = read(fd_in, &b, 1);
        if (n < 1)
        {
            if (waitack)
            {
                gettimeofday(&tvr2, &tz);
                dtvr.tv_sec = tvr2.tv_sec - tvr1.tv_sec;
                dtvr.tv_usec = tvr2.tv_usec - tvr1.tv_usec;
                if (dtvr.tv_usec < 0)
                {
                    dtvr.tv_sec--;
                    dtvr.tv_usec += 1000000;
                }

                int tmt = dtvr.tv_sec;

                if (tmt > RDP_RESENT_TIMEOUT)
                {
                    printf("RETRY\n");
                    rdp_retry(conn);
                    tvr1 = tvr2;
                }
            }

            if (waitclose)
            {
                gettimeofday(&tvc2, &tz);
                dtvc.tv_sec = tvc2.tv_sec - tvc1.tv_sec;
                dtvc.tv_usec = tvc2.tv_usec - tvc1.tv_usec;
                if (dtvc.tv_usec < 0)
                {
                    dtvc.tv_sec--;
                    dtvc.tv_usec += 1000000;
                }

                int tmt = dtvc.tv_sec;

                if (tmt > RDP_CLOSE_TIMEOUT)
                {
                    rdp_final_close(conn);
                    tvc1 = tvc2;
                }
            }
            continue;
        }

        if (rand() < RAND_MAX / 100)
	{
	    printf("Data loss\n");
            continue;
	}
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
