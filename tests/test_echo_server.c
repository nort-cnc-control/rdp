#include <sys/socket.h>
#include <sys/time.h>
#include <stdio.h>
#include <rdpos.h>
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

struct timeval tv1, tv2, dtv;
struct timezone tz;
bool waitack = 0;
int acktimeout;

int cnctd = 0;

void send_rdp(struct rdp_connection_s *conn, const uint8_t *data, size_t dlen)
{
    printf("Sending %i bytes\n", dlen);
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
    gettimeofday(&tv1, &tz);
}

void ack_wait_completed(struct rdp_connection_s *conn, uint32_t seq)
{
    waitack = false;
    printf("Waiting ack %i completed\n", seq);
}

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

    rdp_init_connection(&conn, outbuffer, received, send_rdp, connected, closed, data_send_completed, data_received, ack_wait_start, ack_wait_completed);
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
                gettimeofday(&tv2, &tz);
                dtv.tv_sec = tv2.tv_sec - tv1.tv_sec;
                dtv.tv_usec = tv2.tv_usec - tv1.tv_usec;
                if (dtv.tv_usec < 0)
                {
                    dtv.tv_sec--;
                    dtv.tv_usec += 1000000;
                }

                int tmt = dtv.tv_sec;

                if (tmt > acktimeout)
                {
                    printf("RETRY\n");
                    rdp_retry(&conn);
                }
            }
            continue;
        }

        if (!memcmp(inbuffer, "xxx", 3))
        {
            //printf("foo\n");
            continue;
        }

        bool res = rdp_received(&conn, inbuffer);
        printf("Res = %i\n", res);
        if (cnctd)
        {
            cnctd = 0;
            const char hello[] = "Hello, world!";
            rdp_send(&conn, hello, sizeof(hello) - 1);
        }

        if (conn.state == RDP_CLOSE_WAIT)
        {
            rdp_final_close(&conn);
            rdp_listen(&conn, 1);
        }
    }

    printf("Hello message sent.\n");  

    return 0;
}
