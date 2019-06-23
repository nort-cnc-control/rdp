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

timer_t ack_wait_timer;
timer_t close_wait_timer;


int clsd;
int fd_out;
int fd_in;

void ack_timeout(union sigval sig)
{
    printf("RETRY\n");
    pthread_mutex_lock(&communication_lock);
    rdp_retry(conn);
    pthread_mutex_unlock(&communication_lock);
}

void close_timeout(union sigval sig)
{
    pthread_mutex_lock(&communication_lock);
    rdp_close(conn);
    pthread_mutex_unlock(&communication_lock);
    clsd = 1;
}

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
    pthread_cond_signal(&connected_flag);
}

void closed(struct rdp_connection_s *conn)
{
    const struct itimerspec tspec = {
        .it_interval = {
            .tv_sec = 0,
            .tv_nsec = 0,
        },
        .it_value = {
            .tv_sec = 0,
            .tv_nsec = 0,
        },
    };

    timer_settime(close_wait_timer, 0, &tspec, NULL);
    printf("Connection closed\n");
    clsd = 1;
}

void data_send_completed(struct rdp_connection_s *conn)
{
    printf("Data send completed\n");
}

void data_received(struct rdp_connection_s *conn, const uint8_t *buf, size_t len)
{
    printf("Received: %.*s\n", len, buf);
}

void ack_wait_start(struct rdp_connection_s *conn, uint32_t seq)
{
    printf("Waiting ack %i start\n", seq);
    const struct itimerspec tspec = {
        .it_interval = {
            .tv_sec = RDP_RESEND_TIMEOUT / 1000000,
            .tv_nsec = (RDP_RESEND_TIMEOUT % 1000000) * 1e3,
        },
        .it_value = {
            .tv_sec = RDP_RESEND_TIMEOUT / 1000000,
            .tv_nsec = (RDP_RESEND_TIMEOUT % 1000000) * 1e3,
        },
    };

    timer_settime(ack_wait_timer, 0, &tspec, NULL);
}

void ack_wait_completed(struct rdp_connection_s *conn, uint32_t seq)
{
    const struct itimerspec tspec = {
        .it_interval = {
            .tv_sec = 0,
            .tv_nsec = 0,
        },
        .it_value = {
            .tv_sec = 0,
            .tv_nsec = 0,
        },
    };

    timer_settime(ack_wait_timer, 0, &tspec, NULL);
    printf("Waiting ack %i completed\n", seq);
}

void close_wait_start(struct rdp_connection_s *conn)
{
    printf("Waiting for connection close\n");
    const struct itimerspec tspec = {
        .it_interval = {
            .tv_sec = RDP_CLOSE_TIMEOUT / 1000000,
            .tv_nsec = (RDP_CLOSE_TIMEOUT % 1000000) * 1e3,
        },
        .it_value = {
            .tv_sec = RDP_CLOSE_TIMEOUT / 1000000,
            .tv_nsec = (RDP_CLOSE_TIMEOUT % 1000000) * 1e3,
        },
    };

    timer_settime(ack_wait_timer, 0, &tspec, NULL);
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

void *receive(void *arg)
{
    while (!clsd)
    {
        unsigned char b;
        usleep(1000);
        ssize_t n = read(fd_in, &b, 1);
        if (n < 1)
            continue;
        //printf("B");
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

int main(int argc, const char **argv)
{
    pthread_attr_t attr; /* отрибуты потока */

    const char *serial_port = argv[1];
    speed_t brate = B9600;
    int rdp_port = 1;

    fd_out = open(serial_port, O_RDWR);
    fd_in = fd_out;

    struct termios settings;
    tcgetattr(fd_out, &settings);

    cfsetospeed(&settings, brate); /* baud rate */
    settings.c_cflag &= ~PARENB; /* no parity */
    settings.c_cflag &= ~CSTOPB; /* 1 stop bit */
    settings.c_cflag &= ~CSIZE;
    settings.c_cflag |= CS8 | CLOCAL; /* 8 bits */
    settings.c_lflag = ICANON; /* canonical mode */
    settings.c_oflag &= ~OPOST; /* raw output */

    tcsetattr(fd_out, TCSANOW, &settings); /* apply the settings */
    tcflush(fd_out, TCOFLUSH);

    rdpos_init_connection(&sconn, &bufs, &cbs, &scbs);

    struct sigevent ack_evt = {
        .sigev_notify = SIGEV_THREAD,
        .sigev_notify_function = ack_timeout,
    };
    timer_create(CLOCK_REALTIME, &ack_evt, &ack_wait_timer);

    struct sigevent close_evt = {
        .sigev_notify = SIGEV_THREAD,
        .sigev_notify_function = close_timeout,
    };
    timer_create(CLOCK_REALTIME, &close_evt, &close_wait_timer);

    clsd = 0;
    pthread_cond_init(&connected_flag, NULL);
    pthread_mutex_init(&communication_lock, NULL);

    pthread_attr_init(&attr);
    pthread_create(&tid, &attr, receive, NULL);

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
