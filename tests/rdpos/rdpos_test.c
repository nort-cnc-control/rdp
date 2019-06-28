#include <stdio.h>
#include <rdp.h>
#include <rdpos.h>
#include <assert.h>
#include <string.h>

struct rdpos_connection_s sconn1, sconn2;
struct rdp_connection_s conn1, conn2;

uint8_t rdp_recv_buf1[RDP_MAX_SEGMENT_SIZE];
uint8_t rdp_recv_buf2[RDP_MAX_SEGMENT_SIZE];

uint8_t rdp_outbuf1[RDP_MAX_SEGMENT_SIZE];
uint8_t rdp_outbuf2[RDP_MAX_SEGMENT_SIZE];

uint8_t serial_inbuf1[RDP_MAX_SEGMENT_SIZE];
uint8_t serial_inbuf2[RDP_MAX_SEGMENT_SIZE];

#define cblen (RDP_MAX_SEGMENT_SIZE * 10)

uint8_t serial_cb1[cblen];
int cb1cur, cb1last;

uint8_t serial_cb2[cblen];
int cb2cur, cb2last;

static void push_cb1(uint8_t b)
{
    serial_cb1[cb1cur++] = b;
    cb1cur %= cblen;
}

static int pop_cb1(void)
{
    if (cb1last == cb1cur)
        return -1;
    uint8_t res = serial_cb1[cb1last++];
    cb1last %= cblen;
    return res;
}

static void push_cb2(uint8_t b)
{
    serial_cb2[cb2cur++] = b;
    cb2cur %= cblen;
}

static int pop_cb2(void)
{
    if (cb2last == cb2cur)
        return -1;
    uint8_t res = serial_cb2[cb2last++];
    cb2last %= cblen;
    return res;
}

void send_serial(void *arg, const void *data, size_t len)
{
    struct rdpos_connection_s *sconn = arg;
    int i;
    const uint8_t *buf = data;
    if (sconn == &sconn1)
    {
        printf("Connection 1 sends %i bytes: ", len);
        for (i = 0; i < len; i++)
            push_cb1(buf[i]);
    }
    else
    {
        printf("Connection 2 sends %i bytes: ", len);
        for (i = 0; i < len; i++)
            push_cb2(buf[i]);
    }
    for (i = 0; i < len; i++)
    {
        printf("%02X", ((uint8_t*)data)[i]);
    }
    printf("\n");
}

bool receive_serial(struct rdpos_connection_s *sconn)
{
    int c;
    if (sconn == &sconn1)
    {
        while ((c = pop_cb2()) != -1)
        {
            bool res = rdpos_byte_received(sconn, c);
            if (res == false)
                return false;
        }
    }
    else if (sconn == &sconn2)
    {
        while ((c = pop_cb1()) != -1)
        {
            bool res = rdpos_byte_received(sconn, c);
            if (res == false)
                return false;
        }
    }
    return true;
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

static void set_cbs(struct rdpos_connection_s *sconn, struct rdp_connection_s *conn)
{
    rdp_set_closed_cb(conn, closed);
    rdp_set_connected_cb(conn, connected);
    rdp_set_data_received_cb(conn, data_received);
    rdp_set_data_send_completed_cb(conn, data_send_completed);
    rdpos_set_send_cb(sconn, send_serial);
}

void open_connections(void)
{
    rdp_init_connection(&conn1, rdp_outbuf1, rdp_recv_buf1);
    rdp_init_connection(&conn2, rdp_outbuf2, rdp_recv_buf2);
    rdpos_init_connection(&sconn1, &conn1, serial_inbuf1, sizeof(serial_inbuf1));
    rdpos_init_connection(&sconn2, &conn2, serial_inbuf2, sizeof(serial_inbuf2));

    set_cbs(&sconn1, &conn1);
    set_cbs(&sconn2, &conn2);

    printf("C2 - listen\n");
    rdp_listen(&conn2, 1);
    
    printf("C1 - send SYN\n");
    rdp_connect(&conn1, 2, 1);

    printf("C2 - receive SYN\n");
    receive_serial(&sconn2);

    printf("C1 - SYN, ACK receive\n");
    receive_serial(&sconn1);
    
    printf("C2 - ACK receive\n");
    receive_serial(&sconn2);
}

void close_connections(void)
{
    printf("C1 - RST send\n");
    rdp_close(&conn1);
    
    printf("C2 - RST,ACK send\n");
    receive_serial(&sconn2);
    
    printf("C1 - RST,ACK receive\n");
    receive_serial(&sconn1);
    
    printf("C2 - ACK receive\n");
    receive_serial(&sconn2);
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
    rdp_init_connection(&conn1, rdp_outbuf1, rdp_recv_buf1);
    rdp_init_connection(&conn2, rdp_outbuf2, rdp_recv_buf2);
    rdpos_init_connection(&sconn1, &conn1, serial_inbuf1, sizeof(serial_inbuf1));
    rdpos_init_connection(&sconn2, &conn2, serial_inbuf2, sizeof(serial_inbuf2));

    set_cbs(&sconn1, &conn1);
    set_cbs(&sconn2, &conn2);

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
    res = receive_serial(&sconn2);
    // 3.    SYN-SENT <--- <SEQ=1><ACK=1><SYN,ACK> SYN-RCVD
    assert(res);
    assert(conn1.state == RDP_SYN_SENT);
    assert(conn2.state == RDP_SYN_RCVD);
    assert(rcvd == 0);


    printf("C1 - SYN, ACK receive\n");
    res = receive_serial(&sconn1);
    // 4.    OPEN   <SEQ=2><ACK=1> --->    SYN-RCVD
    assert(res);
    assert(conn1.state == RDP_OPEN);
    assert(conn2.state == RDP_SYN_RCVD);
    assert(rcvd == 0);


    printf("C2 - ACK receive\n");
    res = receive_serial(&sconn2);
    // 5.    OPEN       OPEN
    assert(res);
    assert(conn1.state == RDP_OPEN);
    assert(conn2.state == RDP_OPEN);
    assert(rcvd == 0);

    printf("C1 - RST send\n");
    res = rdp_close(&conn1);
    // 6. CLOSE-WAIT    OPEN
    assert(res);
    assert(conn1.state == RDP_ACTIVE_CLOSE_WAIT);
    assert(conn2.state == RDP_OPEN);

    printf("C2 - RST,ACK send\n");
    res = receive_serial(&sconn2);
    // 7. CLOSE-WAIT    CLOSE-WAIT
    assert(res);
    assert(conn1.state == RDP_ACTIVE_CLOSE_WAIT);
    assert(conn2.state == RDP_PASSIVE_CLOSE_WAIT);
    assert(rcvd == 0);

    printf("C1 - RST,ACK receive\n");
    res = receive_serial(&sconn1);
    // 8. CLOSED    CLOSE-WAIT
    assert(res);
    assert(conn1.state == RDP_CLOSED);
    assert(conn2.state == RDP_PASSIVE_CLOSE_WAIT);
    assert(rcvd == 0);


    printf("C2 - ACK receive\n");
    res = receive_serial(&sconn2);
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
    
    res = receive_serial(&sconn2);
    assert(res);
    assert(rcvd == dlen);
    assert(!memcmp(data, rdp_recv_buf2, dlen));
    rcvd = 0;

    res = receive_serial(&sconn1);
    assert(res);
    assert(rcvd == 0);

    printf("*****\n");
    close_connections();
}



int main(void)
{
    test_connect_listen();
    test_data_send();
    return 0;
}
