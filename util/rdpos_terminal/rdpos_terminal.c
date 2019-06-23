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
    int i;
    struct rdpos_connection_s *conn = arg;
    printf("Sending %i bytes\n", dlen);
    for (i = 0; i < dlen; i++)
        printf("%02X ", ((const uint8_t*)data)[i]);
    printf("\n");
    printf("state = %i\n", conn->rdp_conn.state);
    write(fd, data, dlen);
}

void connected(struct rdp_connection_s *conn)
{
    printf("Connected\n");
    cnctd = 1;
    pthread_cond_signal(&connected_flag);
}

void closed(struct rdp_connection_s *conn)
{
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
        ssize_t n = read(fd, &b, 1);
        if (n < 1)
        {
            rdp_clock(conn, 100000UL);
            continue;
        }
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

    fd = open(serial_port, O_RDWR | O_NOCTTY | O_SYNC);

    struct termios tty;
    tcgetattr(fd, &tty);

    cfsetospeed(&tty, brate); /* baud rate */
    cfsetispeed(&tty, brate); /* baud rate */
    
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
    // disable IGNBRK for mismatched speed tests; otherwise receive break
    // as \000 chars
    tty.c_iflag &= ~IGNBRK;         // disable break processing
    tty.c_lflag = 0;                // no signaling chars, no echo,
                                        // no canonical processing
    tty.c_oflag = 0;                // no remapping, no delays
    tty.c_cc[VMIN]  = 0;            // read doesn't block
    tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

    tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
                                        // enable reading
    tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
    tty.c_cflag &= ~CSTOPB;

    tty.c_cc[VMIN]  = 0;            // non block
    tty.c_cc[VTIME] = 1;            // 0.1 seconds read timeout
    
    tcsetattr(fd, TCSANOW, &tty); /* apply the settings */
    tcflush(fd, TCOFLUSH);

    rdpos_init_connection(&sconn, &bufs, &cbs, &scbs);

    clsd = 0;
    pthread_cond_init(&connected_flag, NULL);
    pthread_mutex_init(&communication_lock, NULL);

    pthread_attr_init(&attr);
    pthread_create(&tid, &attr, receive, NULL);

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
