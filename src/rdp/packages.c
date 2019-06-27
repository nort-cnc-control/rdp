#include <rdp.h>
#include <packages.h>
#include <packages_public.h>
#include <string.h>

#define min(a, b) ((a) < (b) ? (a) : (b))
#define RDP_BASE_HEADER_LEN 14

size_t rdp_build_syn_package(uint8_t *buf, uint8_t src, uint8_t dst,
                             uint32_t initial_seq)
{
    const size_t var = RDP_BASE_HEADER_LEN;
    const size_t hlen = var + 6;
    struct rdp_header_s *hdr = (struct rdp_header_s *)buf;
    memset(hdr, 0, sizeof(*hdr));
    hdr->syn = 1;
    hdr->ack = 0;
    hdr->eack = 0;
    hdr->rst = 0;
    hdr->nul = 0;
    hdr->ver = RDP_VERSION;
    hdr->header_length = hlen / 2;
    hdr->source_port = src;
    hdr->destination_port = dst;
    hdr->data_length = 0;
    hdr->sequence_number = initial_seq;
    hdr->acknowledgement_number = 0;
    
    uint16_t *outstanding = (uint16_t *)(buf + var);
    *outstanding = RDP_MAX_OUTSTANGING;

    uint16_t *maxsegsize = (uint16_t *)(buf + var + 2);
    *maxsegsize = RDP_MAX_SEGMENT_SIZE;

    uint16_t *flags = (uint16_t *)(buf + var + 4);
    *flags = RDP_SDM;
    return hlen;
}

size_t rdp_build_synack_package(uint8_t *buf, uint8_t src, uint8_t dst,
                                uint32_t initial_seq, uint32_t rcv_seq)
{
    const size_t var = RDP_BASE_HEADER_LEN;
    const size_t hlen = var + 6;
    struct rdp_header_s *hdr = (struct rdp_header_s *)buf;
    memset(hdr, 0, sizeof(*hdr));
    hdr->syn = 1;
    hdr->ack = 1;
    hdr->eack = 0;
    hdr->rst = 0;
    hdr->nul = 0;
    hdr->ver = RDP_VERSION;
    hdr->header_length = hlen / 2;
    hdr->source_port = src;
    hdr->destination_port = dst;
    hdr->data_length = 0;
    hdr->sequence_number = initial_seq;
    hdr->acknowledgement_number = rcv_seq;
    
    uint16_t *outstanding = (uint16_t *)(buf + var);
    *outstanding = RDP_MAX_OUTSTANGING;

    uint16_t *maxsegsize = (uint16_t *)(buf + var + 2);
    *maxsegsize = RDP_MAX_SEGMENT_SIZE;

    uint16_t *flags = (uint16_t *)(buf + var + 4);
    *flags = RDP_SDM;
    return hlen;
}

size_t rdp_build_ack_package(uint8_t *buf, uint8_t src, uint8_t dst,
                             uint32_t cur_seq, uint32_t rcv_seq,
                             const uint8_t *data, size_t dlen)
{
    const size_t var = RDP_BASE_HEADER_LEN;
    const size_t hlen = var;
    if (hlen + dlen > RDP_MAX_SEGMENT_SIZE)
        return 0;

    struct rdp_header_s *hdr = (struct rdp_header_s *)buf;
    memset(hdr, 0, sizeof(*hdr));
    hdr->syn = 0;
    hdr->ack = 1;
    hdr->eack = 0;
    hdr->rst = 0;
    hdr->nul = 0;
    hdr->ver = RDP_VERSION;
    hdr->header_length = hlen / 2;
    hdr->source_port = src;
    hdr->destination_port = dst;
    hdr->data_length = dlen;

    hdr->sequence_number = cur_seq;
    hdr->acknowledgement_number = rcv_seq;

    if (dlen > 0)
        memcpy(buf + hlen, data, dlen);
    return hlen + dlen;
}

size_t rdp_build_rstack_package(uint8_t *buf, uint8_t src, uint8_t dst,
                                uint32_t cur_seq, uint32_t rcv_seq)
{
    const size_t var = RDP_BASE_HEADER_LEN;
    const size_t hlen = var;
    
    struct rdp_header_s *hdr = (struct rdp_header_s *)buf;
    memset(hdr, 0, sizeof(*hdr));
    hdr->syn = 0;
    hdr->ack = 1;
    hdr->eack = 0;
    hdr->rst = 1;
    hdr->nul = 0;
    hdr->ver = RDP_VERSION;
    hdr->header_length = hlen / 2;
    hdr->source_port = src;
    hdr->destination_port = dst;
    hdr->data_length = 0;

    hdr->sequence_number = cur_seq;
    hdr->acknowledgement_number = rcv_seq;
    return hlen;
}


size_t rdb_build_eack_package(uint8_t *buf, uint8_t src, uint8_t dst,
                              uint32_t cur_seq, uint32_t rcv_seq,
                              uint32_t *acks, size_t nacks,
                              const uint8_t *data, size_t dlen)
{
    nacks = min(nacks, RDP_MAX_OUTSTANGING);
    const size_t var = RDP_BASE_HEADER_LEN;
    const size_t hlen = var + nacks * 4;
    if (hlen + dlen > RDP_MAX_SEGMENT_SIZE)
        return 0;

    struct rdp_header_s *hdr = (struct rdp_header_s *)buf;
    memset(hdr, 0, sizeof(*hdr));
    hdr->syn = 0;
    hdr->ack = 1;
    hdr->eack = 1;
    hdr->rst = 0;
    hdr->nul = 0;
    hdr->ver = RDP_VERSION;
    hdr->header_length = hlen / 2;
    hdr->source_port = src;
    hdr->destination_port = dst;
    hdr->data_length = dlen;

    hdr->sequence_number = cur_seq;
    hdr->acknowledgement_number = rcv_seq;

    int i;
    uint32_t *backs = (uint32_t *)(buf + var);
    for (i = 0; i < nacks; i++)
    {
        backs[i] = acks[i];
    }

    if (dlen > 0)
        memcpy(buf + hlen, data, dlen);
    return hlen + dlen;
}

size_t rdb_build_rst_package(uint8_t *buf, uint8_t src, uint8_t dst,
                             uint32_t cur_seq, uint32_t rcv_seq)
{
    const size_t var = RDP_BASE_HEADER_LEN;
    const size_t hlen = var;
    struct rdp_header_s *hdr = (struct rdp_header_s *)buf;
    memset(hdr, 0, sizeof(*hdr));
    hdr->syn = 0;
    hdr->ack = 0;
    hdr->eack = 0;
    hdr->rst = 1;
    hdr->nul = 0;
    hdr->ver = RDP_VERSION;
    hdr->header_length = hlen / 2;
    hdr->source_port = src;
    hdr->destination_port = dst;
    hdr->data_length = 0;
    hdr->sequence_number = cur_seq;
    hdr->acknowledgement_number = rcv_seq;
    return hlen;
}

size_t rdp_build_nul_package(uint8_t *buf, uint8_t src, uint8_t dst,
                             uint32_t cur_seq, uint32_t ack)
{
    const size_t var = RDP_BASE_HEADER_LEN;
    const size_t hlen = var;
    struct rdp_header_s *hdr = (struct rdp_header_s *)buf;
    memset(hdr, 0, sizeof(*hdr));
    hdr->syn = 0;
    hdr->ack = 0;
    hdr->eack = 0;
    hdr->rst = 0;
    hdr->nul = 1;
    hdr->ver = RDP_VERSION;
    hdr->header_length = hlen / 2;
    hdr->source_port = src;
    hdr->destination_port = dst;
    hdr->data_length = 0;
    hdr->sequence_number = cur_seq;
    hdr->acknowledgement_number = ack;
    return hlen;
}

void rdb_package_source_destination(const uint8_t *buf, uint8_t *src, uint8_t *dst)
{
    const struct rdp_header_s *hdr = (const struct rdp_header_s *)buf;
    *src = hdr->source_port;
    *dst = hdr->destination_port;
}

enum rdp_package_type_e rdp_package_type(const uint8_t *buf)
{
    const struct rdp_header_s *hdr = (const struct rdp_header_s *)buf;
    if (hdr->ack)
    {
        if (hdr->eack)
            return RDP_EACK;
        if (hdr->rst)
            return RDP_RSTACK;
        if (hdr->nul)
            return RDP_NULACK;
        if (hdr->syn)
            return RDP_SYNACK;
        return RDP_ACK;
    }
    if (hdr->syn)
    {
        return RDP_SYN;
    }
    if (hdr->rst)
    {
        return RDP_RST;
    }
    if (hdr->nul)
    {
        return RDP_NUL;
    }
    return RDP_INVALID;
}
