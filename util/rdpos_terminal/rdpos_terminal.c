#include <stdio.h>
#include <string.h>
#include <rdpos.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <pthread.h>
#include <termios.h>

pthread_mutex_t communication_lock;

pthread_cond_t connected_flag;
pthread_mutex_t mp;
int cnctd = 0;

static struct rdpos_connection_s sconn;
static struct rdp_connection_s *conn = &sconn.rdp_conn;

int clsd;
int fd;

void send_serial(void *arg, const void *data, size_t dlen)
{
    int i;
    struct rdpos_connection_s *conn = arg;
    //printf("\nSending %i bytes\n", dlen);
    //for (i = 0; i < dlen; i++)
    //    printf("%02X ", ((const uint8_t*)data)[i]);
    //printf("\n");
    //printf("state = %i\n", conn->rdp_conn.state);
    write(fd, data, dlen);
}

void connected(struct rdp_connection_s *conn)
{
    printf("\nConnected\n");
    cnctd = 1;
    pthread_cond_signal(&connected_flag);
}

void closed(struct rdp_connection_s *conn)
{
    printf("\nConnection closed\n");
    clsd = 1;
}

void data_send_completed(struct rdp_connection_s *conn)
{
    printf("\nData send completed\n");
}

void data_received(struct rdp_connection_s *conn, const uint8_t *buf, size_t len)
{
    printf("\nReceived: %.*s\n", len, buf);
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

void *receive(void *arg)
{
    printf("Starting recv thread\n");
    while (!clsd)
    {
        unsigned char b;
        int n = read(fd, &b, 1);
        if (n < 1)
        {
            usleep(100);
            continue;
        }
        //printf("%02X ", b);
        //fflush(stdout);
        pthread_mutex_lock(&communication_lock);
        bool res = rdpos_byte_received(&sconn, b);
        pthread_mutex_unlock(&communication_lock);
        if (conn->state == RDP_CLOSED)
        {
            printf("Exit\n");
            break;
        }
    }
    return NULL;
}

pthread_t tid; /* идентификатор потока */

void handle_exit(int signo)
{
    if (signo == SIGINT)
    {
        printf("received SIGINT\n");
        pthread_mutex_lock(&communication_lock);
        rdp_close(conn);
        pthread_mutex_unlock(&communication_lock);
        pthread_join(tid, NULL);
        pthread_mutex_destroy(&communication_lock);
        exit(0);
    }
}

void rdpclock(union sigval sig)
{
    rdp_clock(conn, 1000);
}

int main(int argc, const char **argv)
{
    pthread_attr_t attr; /* отрибуты потока */

    const char *serial_port = argv[1];
    speed_t brate = B115200;
    int rdp_port = 1;

    printf("Opening %s\n", serial_port);
    fd = open(serial_port, O_RDWR | O_NOCTTY | O_NDELAY);

    if (fd <= 0)
    {
        printf("Error opening\n");
        return 0;
    }

    rdpos_init_connection(&sconn, &bufs, &cbs, &scbs);

    clsd = 0;
    pthread_cond_init(&connected_flag, NULL);
    pthread_mutex_init(&communication_lock, NULL);

    pthread_attr_init(&attr);
    pthread_create(&tid, &attr, receive, NULL);

    timer_t timer;
    struct sigevent sevt = {
        .sigev_notify = SIGEV_THREAD,
        .sigev_notify_function = rdpclock,
    };
    timer_create(CLOCK_REALTIME, &sevt, &timer);
    struct itimerspec interval = {
        .it_interval = {
            .tv_sec = 0,
            .tv_nsec = 1000000UL,
        },
        .it_value = {
            .tv_sec = 0,
            .tv_nsec = 1000000UL,
        }
    };
    timer_settime(timer, 0, &interval, NULL);

    usleep(100000);

    pthread_mutex_lock(&communication_lock);
    rdp_connect(conn, 1, 1);
    pthread_mutex_unlock(&communication_lock);

    pthread_cond_wait(&connected_flag, &mp);

    signal(SIGINT, handle_exit);

    while (true)
    {
        char pbuf[1000];
        printf("> ");
        scanf("%s", pbuf);
        pthread_mutex_lock(&communication_lock);
        rdp_send(conn, pbuf, strlen(pbuf));
        pthread_mutex_unlock(&communication_lock);
    }

    return 0;
}
