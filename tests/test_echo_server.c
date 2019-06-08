#include <sys/socket.h>
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

int cnctd = 0;

void send_rdp(struct rdp_connection_s *conn, const uint8_t *data, size_t dlen)
{
    printf("Sending %i bytes\n", dlen);
    //if (rand() > RAND_MAX / 5)
        sendto(fd, data, dlen, MSG_CONFIRM, (struct sockaddr *) &cliaddr, clientaddrlen);
    //else
    //    printf("Package loss\n");
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

int main(void)
{
    fd = socket(AF_INET, SOCK_DGRAM, 0);

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

    rdp_init_connection(&conn, outbuffer, received, send_rdp, connected, closed, data_send_completed, data_received);
    rdp_listen(&conn, 1);

    while (true)
    {
        int n;
        n = recvfrom(fd, inbuffer, RDP_MAX_SEGMENT_SIZE,  
                     MSG_WAITALL, (struct sockaddr *)&cliaddr, 
                     &clientaddrlen);
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
