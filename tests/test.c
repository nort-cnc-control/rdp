#include <stdio.h>
#include <rdpos.h>
#include <assert.h>
#include <string.h>

struct rdp_connection_s conn1, conn2;
uint8_t inbuf1[RDP_MAX_SEGMENT_SIZE], inbuf2[RDP_MAX_SEGMENT_SIZE];
uint8_t outbuf1[RDP_MAX_SEGMENT_SIZE], outbuf2[RDP_MAX_SEGMENT_SIZE];
uint8_t tmp1[RDP_MAX_SEGMENT_SIZE], tmp2[RDP_MAX_SEGMENT_SIZE];

void send1(struct rdp_connection_s *conn, const uint8_t *data, size_t len)
{
    printf("Connection 1 sends %i bytes: ", len);
    int i;
    for (i = 0; i < len; i++)
    {
        printf("%02X", data[i]);
    }
    printf("\n");
}

void send2(struct rdp_connection_s *conn, const uint8_t *data, size_t len)
{
    printf("Connection 2 sends %i bytes: ", len);
    int i;
    for (i = 0; i < len; i++)
    {
        printf("%02X", data[i]);
    }
    printf("\n");
}

void connected(struct rdp_connection_s *conn)
{
    if (conn == &conn1)
    {
        printf("Connection 1 connected\n");
    }
    else
    {
        printf("Connection 2 connected\n");
    }
}

void closed(struct rdp_connection_s *conn)
{
    if (conn == &conn1)
    {
        printf("Connection 1 closed\n");
    }
    else
    {
        printf("Connection 2 closed\n");
    }
}

void data_send_completed(struct rdp_connection_s *conn)
{
    if (conn == &conn1)
    {
        printf("Connection 1 send completed\n");
    }
    else
    {
        printf("Connection 2 send completed\n");
    }
}

size_t rcvd;

void data_received(struct rdp_connection_s *conn, const uint8_t *buf, size_t len)
{
    int i;
    if (conn == &conn1)
    {
        printf("Connection 1 received ");
    }
    else
    {
        printf("Connection 2 received ");
    }
    for (i = 0; i < len; i++)
    {
        printf("%02X", buf[i]);
    }
    printf("\n");
    rcvd = len;
}

void open_connections(void)
{
    rdp_init_connection(&conn1, outbuf1, inbuf1, send1, connected, closed, data_send_completed, data_received);
    rdp_init_connection(&conn2, outbuf2, inbuf2, send2, connected, closed, data_send_completed, data_received);

    printf("C2 - listen\n");
    rdp_listen(&conn2, 1);
    
    printf("C1 - send SYN\n");
    rdp_connect(&conn1, 2, 1);
    
    size_t rcvd;
    printf("C2 - receive SYN\n");
    rdp_received(&conn2, outbuf1);
    
    printf("C1 - SYN, ACK receive\n");
    rdp_received(&conn1, outbuf2);
    
    printf("C2 - ACK receive\n");
    rdp_received(&conn2, outbuf1);
}

void close_connecions()
{
    size_t rcvd;
    printf("C1 - RST send\n");
    rdp_close(&conn1);
    
    printf("C2 - RST receive\n");
    rdp_received(&conn2, outbuf1);
    
    printf("C1 - ACK receive\n");
    rdp_received(&conn1, outbuf2);

    printf("C1 - final close\n");
    rdp_final_close(&conn1);
    
    printf("C2 - final close\n");
    rdp_final_close(&conn2);
}

void test_connect_listen(void)
{
    printf("\nTEST: Connect : Listen\n\n");
    /*
     *             Host A                         Host B
     * 
     * Time   State                                             State
     * 
     * 1.    CLOSED                                             LISTEN
     * 
     * 2.    SYN-SENT    <SEQ=100><SYN> --->
     * 
     * 3.                               <--- <SEQ=200><ACK=100><SYN,ACK>
     *                                                         SYN-RCVD
     * 
     * 4.    OPEN    <SEQ=101><ACK=200> --->                    OPEN
     */
    bool res;
    rdp_init_connection(&conn1, outbuf1, inbuf1, send1, connected, closed, data_send_completed, data_received);
    rdp_init_connection(&conn2, outbuf2, inbuf2, send2, connected, closed, data_send_completed, data_received);

    printf("C2 - listen\n");
    res = rdp_listen(&conn2, 1);
    // 1.    CLOSED     LISTEN
    assert(res);
    assert(conn1.state == RDP_CLOSED);
    assert(conn2.state == RDP_LISTEN);

    printf("C1 - send SYN\n");
    
    res = rdp_connect(&conn1, 2, 1);
    // 2.    SYN-SENT  <SEQ=1><SYN> --->  LISTEN
    assert(res);
    assert(conn1.state == RDP_SYN_SENT);
    assert(conn2.state == RDP_LISTEN);

    printf("C2 - receive SYN\n");
    res = rdp_received(&conn2, outbuf1);
    // 3.    SYN-SENT <--- <SEQ=1><ACK=1><SYN,ACK> SYN-RCVD
    assert(res);
    assert(conn1.state == RDP_SYN_SENT);
    assert(conn2.state == RDP_SYN_RCVD);
    assert(rcvd == 0);

    printf("C1 - SYN, ACK receive\n");
    res = rdp_received(&conn1, outbuf2);
    // 4.    OPEN   <SEQ=2><ACK=1> --->    SYN-RCVD
    assert(res);
    assert(conn1.state == RDP_OPEN);
    assert(conn2.state == RDP_SYN_RCVD);
    assert(rcvd == 0);

    printf("C2 - ACK receive\n");
    res = rdp_received(&conn2, outbuf1);
    // 5.    OPEN       OPEN
    assert(res);
    assert(conn1.state == RDP_OPEN);
    assert(conn2.state == RDP_OPEN);
    assert(rcvd == 0);

    
    printf("C1 - RST send\n");
    res = rdp_close(&conn1);
    // 6. CLOSE-WAIT    OPEN
    assert(res);
    assert(conn1.state == RDP_CLOSE_WAIT);
    assert(conn2.state == RDP_OPEN);

    printf("C2 - RST receive\n");
    res = rdp_received(&conn2, outbuf1);
    // 7. CLOSE-WAIT    CLOSE-WAIT
    assert(res);
    assert(conn1.state == RDP_CLOSE_WAIT);
    assert(conn2.state == RDP_CLOSE_WAIT);
    assert(rcvd == 0);

    printf("C1 - final close\n");
    res = rdp_final_close(&conn1);
    // 8. CLOSED        CLOSE-WAIT
    assert(res);
    assert(conn1.state == RDP_CLOSED);
    assert(conn2.state == RDP_CLOSE_WAIT);
    
    printf("C2 - final close\n");
    res = rdp_final_close(&conn2);
    // 9. CLOSED        CLOSED
    assert(res);
    assert(conn1.state == RDP_CLOSED);
    assert(conn2.state == RDP_CLOSED);
}

void test_connect_connect_1(void)
{
    printf("\nTEST: Connect : Connect 1\n\n");
    bool res;
    rdp_init_connection(&conn1, outbuf1, inbuf1, send1, connected, closed, data_send_completed, data_received);
    rdp_init_connection(&conn2, outbuf2, inbuf2, send2, connected, closed, data_send_completed, data_received);

    printf("C1 - send SYN\n");
    res = rdp_connect(&conn1, 2, 1);
    assert(res);
    assert(conn1.state == RDP_SYN_SENT);
    assert(conn2.state == RDP_CLOSED);

    printf("C2 - send SYN\n");
    res = rdp_connect(&conn2, 1, 2);
    assert(res);
    assert(conn1.state == RDP_SYN_SENT);
    assert(conn2.state == RDP_SYN_SENT);

    memcpy(tmp1, outbuf1, RDP_MAX_SEGMENT_SIZE);
    memcpy(tmp2, outbuf2, RDP_MAX_SEGMENT_SIZE);

    printf("C2 - receive SYN and send SYN,ACK\n");
    res = rdp_received(&conn2, tmp1);
    assert(res);
    assert(conn1.state == RDP_SYN_SENT);
    assert(conn2.state == RDP_SYN_RCVD);
    assert(rcvd == 0);

    printf("C1 - receive SYN and send SYN,ACK\n");
    res = rdp_received(&conn1, tmp2);
    assert(res);
    assert(conn1.state == RDP_SYN_RCVD);
    assert(conn2.state == RDP_SYN_RCVD);
    assert(rcvd == 0);

    memcpy(tmp1, outbuf1, RDP_MAX_SEGMENT_SIZE);
    memcpy(tmp2, outbuf2, RDP_MAX_SEGMENT_SIZE);

    printf("C2 - SYN,ACK receive\n");
    res = rdp_received(&conn2, tmp1);
    assert(res);
    assert(conn1.state == RDP_SYN_RCVD);
    assert(conn2.state == RDP_OPEN);
    assert(rcvd == 0);

    printf("C1 - SYN,ACK receive\n");
    res = rdp_received(&conn1, tmp2);
    assert(res);
    assert(conn1.state == RDP_OPEN);
    assert(conn2.state == RDP_OPEN);
    assert(rcvd == 0);


    printf("C1 - RST send\n");
    res = rdp_close(&conn1);
    // 6. CLOSE-WAIT    OPEN
    assert(res);
    assert(conn1.state == RDP_CLOSE_WAIT);
    assert(conn2.state == RDP_OPEN);

    printf("C2 - RST receive\n");
    res = rdp_received(&conn2, outbuf1);
    // 7. CLOSE-WAIT    CLOSE-WAIT
    assert(res);
    assert(conn1.state == RDP_CLOSE_WAIT);
    assert(conn2.state == RDP_CLOSE_WAIT);
    assert(rcvd == 0);

    printf("C1 - final close\n");
    res = rdp_final_close(&conn1);
    // 8. CLOSED        CLOSE-WAIT
    assert(res);
    assert(conn1.state == RDP_CLOSED);
    assert(conn2.state == RDP_CLOSE_WAIT);
    
    printf("C2 - final close\n");
    res = rdp_final_close(&conn2);
    // 9. CLOSED        CLOSED
    assert(res);
    assert(conn1.state == RDP_CLOSED);
    assert(conn2.state == RDP_CLOSED);
}

void test_connect_connect_2(void)
{
    printf("\nTEST: Connect : Connect 2\n\n");
    bool res;
    rdp_init_connection(&conn1, outbuf1, inbuf1, send1, connected, closed, data_send_completed, data_received);
    rdp_init_connection(&conn2, outbuf2, inbuf2, send2, connected, closed, data_send_completed, data_received);

    printf("C1 - send SYN\n");
    res = rdp_connect(&conn1, 2, 1);
    assert(res);
    assert(conn1.state == RDP_SYN_SENT);
    assert(conn2.state == RDP_CLOSED);

    printf("C2 - receive SYN\n");
    res = rdp_received(&conn2, outbuf1);
    assert(res == 0);
    assert(conn1.state == RDP_SYN_SENT);
    assert(conn2.state == RDP_CLOSED);
    assert(rcvd == 0);

    printf("C2 - send SYN\n");
    res = rdp_connect(&conn2, 1, 2);
    assert(res);
    assert(conn1.state == RDP_SYN_SENT);
    assert(conn2.state == RDP_SYN_SENT);

    printf("C1 - receive SYN\n");
    res = rdp_received(&conn1, outbuf2);
    assert(res);
    assert(conn1.state == RDP_SYN_RCVD);
    assert(conn2.state == RDP_SYN_SENT);
    assert(rcvd == 0);

    printf("C2 - receive SYN,ACK\n");
    res = rdp_received(&conn2, outbuf1);
    assert(res);
    assert(conn1.state == RDP_SYN_RCVD);
    assert(conn2.state == RDP_OPEN);
    assert(rcvd == 0);

    printf("C1 - ACK receive\n");
    res = rdp_received(&conn1, outbuf2);
    assert(res);
    assert(conn1.state == RDP_OPEN);
    assert(conn2.state == RDP_OPEN);
    assert(rcvd == 0);

    printf("C1 - RST send\n");
    res = rdp_close(&conn1);
    // 6. CLOSE-WAIT    OPEN
    assert(res);
    assert(conn1.state == RDP_CLOSE_WAIT);
    assert(conn2.state == RDP_OPEN);

    printf("C2 - RST receive\n");
    res = rdp_received(&conn2, outbuf1);
    // 7. CLOSE-WAIT    CLOSE-WAIT
    assert(res);
    assert(conn1.state == RDP_CLOSE_WAIT);
    assert(conn2.state == RDP_CLOSE_WAIT);
    assert(rcvd == 0);

    printf("C1 - final close\n");
    res = rdp_final_close(&conn1);
    // 8. CLOSED        CLOSE-WAIT
    assert(res);
    assert(conn1.state == RDP_CLOSED);
    assert(conn2.state == RDP_CLOSE_WAIT);
    
    printf("C2 - final close\n");
    res = rdp_final_close(&conn2);
    // 9. CLOSED        CLOSED
    assert(res);
    assert(conn1.state == RDP_CLOSED);
    assert(conn2.state == RDP_CLOSED);
}

void test_connect_connect_3(void)
{
    printf("\nTEST: Connect : Connect 3\n\n");
    bool res;
    rdp_init_connection(&conn1, outbuf1, inbuf1, send1, connected, closed, data_send_completed, data_received);
    rdp_init_connection(&conn2, outbuf2, inbuf2, send2, connected, closed, data_send_completed, data_received);

    printf("C1 - send SYN\n");
    res = rdp_connect(&conn1, 2, 1);
    assert(res);
    assert(conn1.state == RDP_SYN_SENT);
    assert(conn2.state == RDP_CLOSED);

    printf("C2 - receive SYN\n");
    res = rdp_received(&conn2, outbuf1);
    assert(res == 0);
    assert(conn1.state == RDP_SYN_SENT);
    assert(conn2.state == RDP_CLOSED);
    assert(rcvd == 0);

    // Reinit conn1
    rdp_reset_connection(&conn1);
    printf("C1 - send SYN\n");
    res = rdp_connect(&conn1, 2, 1);
    assert(res);
    assert(conn1.state == RDP_SYN_SENT);
    assert(conn2.state == RDP_CLOSED);

    printf("C2 - send SYN\n");
    res = rdp_connect(&conn2, 1, 2);
    assert(res);
    assert(conn1.state == RDP_SYN_SENT);
    assert(conn2.state == RDP_SYN_SENT);

    printf("C1 - receive SYN\n");
    res = rdp_received(&conn1, outbuf2);
    assert(res);
    assert(conn1.state == RDP_SYN_RCVD);
    assert(conn2.state == RDP_SYN_SENT);
    assert(rcvd == 0);

    printf("C2 - receive SYN,ACK\n");
    res = rdp_received(&conn2, outbuf1);
    assert(res);
    assert(conn1.state == RDP_SYN_RCVD);
    assert(conn2.state == RDP_OPEN);
    assert(rcvd == 0);

    printf("C1 - ACK receive\n");
    res = rdp_received(&conn1, outbuf2);
    assert(res);
    assert(conn1.state == RDP_OPEN);
    assert(conn2.state == RDP_OPEN);
    assert(rcvd == 0);

    printf("C1 - RST send\n");
    res = rdp_close(&conn1);
    // 6. CLOSE-WAIT    OPEN
    assert(res);
    assert(conn1.state == RDP_CLOSE_WAIT);
    assert(conn2.state == RDP_OPEN);

    printf("C2 - RST receive\n");
    res = rdp_received(&conn2, outbuf1);
    // 7. CLOSE-WAIT    CLOSE-WAIT
    assert(res);
    assert(conn1.state == RDP_CLOSE_WAIT);
    assert(conn2.state == RDP_CLOSE_WAIT);
    assert(rcvd == 0);

    printf("C1 - final close\n");
    res = rdp_final_close(&conn1);
    // 8. CLOSED        CLOSE-WAIT
    assert(res);
    assert(conn1.state == RDP_CLOSED);
    assert(conn2.state == RDP_CLOSE_WAIT);
    
    printf("C2 - final close\n");
    res = rdp_final_close(&conn2);
    // 9. CLOSED        CLOSED
    assert(res);
    assert(conn1.state == RDP_CLOSED);
    assert(conn2.state == RDP_CLOSED);
}

void test_data_send(void)
{
    bool res;
    int i;
    printf("\nTEST: data send\n\n");
    open_connections();
    
    printf("*****\n");
    uint8_t data[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    size_t dlen = sizeof(data);

    res = rdp_send(&conn1, data, dlen);
    assert(res);
    
    res = rdp_received(&conn2, outbuf1);
    assert(res);
    assert(rcvd == dlen);
    assert(!memcmp(data, inbuf2, dlen));
    rcvd = 0;

    res = rdp_received(&conn1, outbuf2);
    assert(res);
    assert(rcvd == 0);

    printf("*****\n");
    close_connecions();
}

void test_data_send_packet_lost_1(void)
{
    bool res;
    int i;
    printf("\nTEST: data send packet lost 1\n\n");
    open_connections();
    
    printf("*****\n");
    uint8_t data[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    size_t dlen = sizeof(data);

    res = rdp_send(&conn1, data, dlen);
    assert(res);
    
    // package lost

    res = rdp_retry(&conn1);
    assert(res);

    res = rdp_received(&conn2, outbuf1);
    assert(res);
    assert(rcvd == dlen);
    assert(!memcmp(data, inbuf2, dlen));
    rcvd = 0;

    res = rdp_received(&conn1, outbuf2);
    assert(res);
    assert(rcvd == 0);

    printf("*****\n");
    close_connecions();
}

void test_data_send_packet_lost_2(void)
{
    bool res;
    int i;
    printf("\nTEST: data send packet lost 2\n\n");
    open_connections();
    
    printf("*****\n");
    uint8_t data[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    size_t dlen = sizeof(data);

    res = rdp_send(&conn1, data, dlen);
    assert(res);
    
    res = rdp_received(&conn2, outbuf1);
    assert(res);
    assert(rcvd == dlen);
    assert(!memcmp(data, inbuf2, dlen));
    rcvd = 0;

    // ack package lost
    // C1 resends data
    res = rdp_retry(&conn1);
    assert(res);

    // C2 receives data again, but must ignore it, only ACK send
    res = rdp_received(&conn2, outbuf1);
    assert(res);
    assert(rcvd == 0);

    res = rdp_received(&conn1, outbuf2);
    assert(res);
    assert(rcvd == 0);

    printf("*****\n");
    close_connecions();
}

int main(void)
{
    test_connect_listen();
    test_connect_connect_1();
    test_connect_connect_2();
    test_connect_connect_3();
    test_data_send();
    test_data_send_packet_lost_1();
    test_data_send_packet_lost_2();
    return 0;
}
