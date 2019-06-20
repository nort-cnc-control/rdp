#include <sys/socket.h>
#include <sys/time.h>
#include <stdio.h>
#include <rdp.h>
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 

const int PORT = 9000;
int fd;
struct sockaddr_in servaddr;;

uint8_t inbuffer[RDP_MAX_SEGMENT_SIZE];
uint8_t outbuffer[RDP_MAX_SEGMENT_SIZE];
uint8_t received[RDP_MAX_SEGMENT_SIZE];
size_t lenrecv;

struct timeval tv1, tv2, dtv;
struct timezone tz;
bool waitack = false;
bool waitclose = false;

int cnctd = 0;

void send_rdp(struct rdp_connection_s *conn, const uint8_t *data, size_t dlen)
{
    printf("Sending %i bytes\n", dlen);
    printf("state = %i\n", conn->state);
    if (rand() > RAND_MAX / 5)
        sendto(fd, data, dlen, MSG_CONFIRM, (const struct sockaddr *) &servaddr, sizeof(servaddr));
    else
        printf("Package loss\n");
}

void connected(struct rdp_connection_s *conn)
{
    printf("connected\n");
    cnctd = 1;
}

void closed(struct rdp_connection_s *conn)
{
    printf("closed\n");
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
    gettimeofday(&tv1, &tz);
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
}

struct rdp_cbs_s cbs = {
    .send = send_rdp,
    .connected = connected,
    .closed = closed,
    .data_send_completed = data_send_completed,
    .data_received = data_received,
    .ack_wait_start = ack_wait_start,
    .ack_wait_completed = ack_wait_completed,
    .close_wait_start = close_wait_start,
};


int main(void)
{
    int n;
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));


    memset(&servaddr, 0, sizeof(servaddr)); 
      
    // Filling server information 
    servaddr.sin_family    = AF_INET; // IPv4 
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
    servaddr.sin_port = htons(PORT); 

    struct rdp_connection_s conn;

    sendto(fd, "xxx", 3, MSG_CONFIRM, (struct sockaddr *) &servaddr, sizeof(servaddr));

    rdp_init_connection(&conn, outbuffer, received, &cbs, NULL);
    rdp_connect(&conn, 1, 1);

    while (conn.state != RDP_CLOSED)
    {
        socklen_t socklen;
        int n = recv(fd, inbuffer, RDP_MAX_SEGMENT_SIZE, MSG_WAITALL); 

        if (n < 1)
        {
            if (waitack)
            {
                gettimeofday(&tv2, &tz);
                dtv.tv_sec = tv2.tv_sec - tv1.tv_sec;
                dtv.tv_usec = tv2.tv_usec - tv1.tv_usec;
                if (dtv.tv_usec < 0)
                {
                    dtv.tv_sec--;
                    dtv.tv_usec += 1000000;
                }

                int tmt = dtv.tv_sec;

                if (tmt > RDP_RESEND_TIMEOUT)
                {
                    printf("RETRY\n");
                    rdp_retry(&conn);
                    tv1 = tv2;
                }
            }
            continue;
        }

        printf("state = %i\n", conn.state);
        bool res = rdp_received(&conn, inbuffer);
        printf("Res = %i\n", res);
        if (lenrecv > 0)
        {
            rdp_close(&conn);
            lenrecv = 0;
        }
    }
    
    return 0;
}
