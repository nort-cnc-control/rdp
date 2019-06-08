#pragma once

#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <defs.h>

enum rdp_package_type_e {
    RDP_SYN = 0,
    RDP_ACK,
    RDP_SYNACK,
    RDP_EACK,
    RDP_NUL,
    RDP_NULACK,
    RDP_RST,
    RDP_INVALID,
};

struct __attribute__((__packed__)) rdp_header_s {
    struct {
        uint8_t syn : 1;
        uint8_t ack : 1;
        uint8_t eack : 1;
        uint8_t rst : 1;
        uint8_t nul : 1;
        uint8_t _zero : 1;
        uint8_t ver : 2;
    };
    uint8_t header_length;
    uint8_t source_port;
    uint8_t destination_port;
    uint16_t data_length;
    uint32_t sequence_number;
    uint32_t acknowledgement_number;
    uint32_t checksum;
};

size_t rdp_build_syn_package(uint8_t *buf, uint8_t src, uint8_t dst,
                             uint32_t initial_seq);

size_t rdp_build_synack_package(uint8_t *buf, uint8_t src, uint8_t dst,
                                uint32_t initial_seq, uint32_t rcv_seq);

size_t rdp_build_ack_package(uint8_t *buf, uint8_t src, uint8_t dst,
                             uint32_t cur_seq, uint32_t rcv_seq,
                             const uint8_t *data, size_t dlen);

size_t rdb_build_eack_package(uint8_t *buf, uint8_t src, uint8_t dst,
                              uint32_t cur_seq, uint32_t rcv_seq,
                              uint32_t *acks, size_t nacks,
                              const uint8_t *data, size_t dlen);

size_t rdb_build_rst_package(uint8_t *buf, uint8_t src, uint8_t dst,
                             uint32_t cur_seq);

size_t rdb_build_nul_package(uint8_t *buf, uint8_t src, uint8_t dst,
                             uint32_t cur_seq);

enum rdp_package_type_e rdp_package_type(const uint8_t *buf);
