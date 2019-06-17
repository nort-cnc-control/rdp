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
struct sockaddr_in servaddr, cliaddr;
int clientaddrlen;

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

void send_rdp(struct rdp_connection_s *conn, const uint8_t *data, size_t dlen)
{
    printf("Sending %i bytes\n", dlen);
    printf("state = %i\n", conn->state);
    if (rand() > RAND_MAX / 5)
        sendto(fd, data, dlen, MSG_CONFIRM, (struct sockaddr *) &cliaddr, clientaddrlen);
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
    waitack = false;
    waitclose = false;
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
    fd = socket(AF_INET, SOCK_DGRAM, 0);

    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    memset(&servaddr, 0, sizeof(servaddr)); 
    memset(&cliaddr, 0, sizeof(cliaddr)); 
      
    // Filling server information 
    servaddr.sin_family    = AF_INET; // IPv4 
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
    servaddr.sin_port = htons(PORT); 

    // Bind the socket with the server address 
    if (bind(fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) 
    { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 

    struct rdp_connection_s conn;

    rdp_init_connection(&conn, outbuffer, received, &cbs, NULL);
    rdp_listen(&conn, 1);

    while (true)
    {
        int n;
        n = recvfrom(fd, inbuffer, RDP_MAX_SEGMENT_SIZE,  
                     MSG_WAITALL, (struct sockaddr *)&cliaddr, 
                     &clientaddrlen);

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
                    rdp_retry(&conn);
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
                    rdp_final_close(&conn);
                    tvc1 = tvc2;
                }
            }
            continue;
        }

        if (!memcmp(inbuffer, "xxx", 3))
        {
            //printf("foo\n");
            continue;
        }

        printf("state = %i\n", conn.state);
        bool res = rdp_received(&conn, inbuffer);
        printf("Res = %i\n", res);
        if (cnctd)
        {
            cnctd = 0;
            const char hello[] = "Hello, world!";
            rdp_send(&conn, hello, sizeof(hello) - 1);
        }

        if (conn.state == RDP_CLOSED)
        {
            rdp_listen(&conn, 1);
        }
    }

    printf("Hello message sent.\n");  

    return 0;
}
