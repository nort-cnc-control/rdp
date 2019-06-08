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
struct sockaddr_in servaddr;;

uint8_t inbuffer[RDP_MAX_SEGMENT_SIZE];
uint8_t outbuffer[RDP_MAX_SEGMENT_SIZE];
uint8_t received[RDP_MAX_SEGMENT_SIZE];
size_t lenrecv;

int cnctd = 0;

void send_rdp(struct rdp_connection_s *conn, const uint8_t *data, size_t dlen)
{
    printf("Sending bytes to server\n");
    //if (rand() > RAND_MAX / 5)
        sendto(fd, data, dlen, MSG_CONFIRM, (const struct sockaddr *) &servaddr, sizeof(servaddr));
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
    lenrecv = len;
}


int main(void)
{
    int n;
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    
    memset(&servaddr, 0, sizeof(servaddr)); 
      
    // Filling server information 
    servaddr.sin_family    = AF_INET; // IPv4 
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
    servaddr.sin_port = htons(PORT); 

    struct rdp_connection_s conn;

    sendto(fd, "xxx", 3, MSG_CONFIRM, (struct sockaddr *) &servaddr, sizeof(servaddr));

    rdp_init_connection(&conn, outbuffer, received, send_rdp, connected, closed, data_send_completed, data_received);
    rdp_connect(&conn, 1, 1);

    while (conn.state != RDP_CLOSED)
    {
        socklen_t socklen;
        recv(fd, inbuffer, RDP_MAX_SEGMENT_SIZE, MSG_WAITALL); 
        bool res = rdp_received(&conn, inbuffer);
        printf("Res = %i\n", res);
        if (lenrecv > 0)
        {
            rdp_close(&conn);
            rdp_final_close(&conn);
            lenrecv = 0;
        }
    }
    
    return 0;
}
