#pragma once

// Must be aligned to 4
// Must be > 24
#define RDP_MAX_SEGMENT_SIZE 256

// Require order of packets
#define RDP_SDM 1

// Max out of order packets. Only 0 is supported
#define RDP_MAX_OUTSTANGING 0

// Close timeout
#define RDP_CLOSE_TIMEOUT 6

// Resend timeout
#define RDP_RESENT_TIMEOUT 1

