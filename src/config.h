#pragma once

// Must be aligned to 4
// Must be > 24
#define RDP_MAX_SEGMENT_SIZE 128

// Require order of packets
#define RDP_SDM 1

// Max out of order packets. Only 0 is supported
#define RDP_MAX_OUTSTANGING 0

// Close timeout
#define RDP_CLOSE_TIMEOUT 6000000

// Resend timeout
#define RDP_RESEND_TIMEOUT 100000

// Keepalive timeout
#define RDP_KEEPALIVE_TIMEOUT 10000000

// Keepalive packet send timeout
#define RDP_KEEPALIVE_SEND_TIMEOUT 5000000
