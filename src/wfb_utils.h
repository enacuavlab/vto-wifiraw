#ifndef WFB_UTILS_H
#define WFB_UTILS_H

#include <stdbool.h>
#include <stdint.h>

#if RAW
#else
#include <netinet/in.h>
#endif // RAW

#define ONLINE_MTU 1450

typedef enum { L0_ST, L1_ST, L2_ST, ST_NB } status_t;
// Board :
//  L0_ST : doing nothing 
//  L1_ST : listen for any unknow message during FREESCANPERIOD (20 sec period), otherwize increment channel and listen again
//  L1_ST -> L2_ST : no unknow msg received, lock channel, set on board main first, and backup when available
//  L2_ST : - on board main : send wfbdown(chan) + data. Chan refer to onboard backup chan, equal to -1, until free channel found
//          - on board backup : send wfdown(chan). Chan refer to onboard main chan.
//
//          when receiving fails msg above threshold 
//          - on board main : switch to onboard backup, and scan for another free channel
//          - on board backup : scan for another free channel
//  L2_ST -> L1_ST : number of unknow messages during FREETRANPERIOD (4 sec period), above threshold FREENBPKT (2),
//                  looking for another available channel
//
// Ground : 
//  L0_ST : doing nothing
//  L1_ST : listen for known messages during 2 sec period, otherwize increment channel and listen again
//  L1_ST -> L2_ST : receive wfbdown, lock channels scannings, set ground main for the first
//            and wait for wfbdown(chan != -1) to set backup channel
//            Do not listen the backup channel until is has been set.
//  L2_ST : main receive wfbdown + data, and backup receive wfbdown
//   
//           when detecting board msg  
//           - on ground main : swith to ground backup
//           - on ground backup : wait 
//
//
typedef enum { TIME_FD, RAW0_FD, RAW1_FD, WFB_FD, TUN_FD, VID1_FD, VID2_FD, TEL_FD, FD_NB } cannal_t;

typedef struct {
  uint16_t seq;
  uint16_t len;  // len for the full payload not including (pay_hdr_t)
  uint64_t stp_n;
} __attribute__((packed)) payhdr_t;
// pay = (subpay1 + data1) + (subpay2 + data2) + ...
typedef struct {
  uint8_t id;
  uint16_t len; // len for the subpayload not including (sub_pay_hdr_t)
} __attribute__((packed)) subpayhdr_t;

typedef struct {
  int8_t chan;
} __attribute__((packed)) wfbdown_t;

#define freqsmax 65

typedef struct {
  char name[20];
  uint8_t freqsnb;
  uint8_t freqcptcur;
  uint32_t freqs[freqsmax];
  uint32_t chans[freqsmax];
  uint16_t fd;
  uint16_t seq_out;
  uint32_t fails;
  uint32_t recvs;
  status_t status;
  bool datatosend;
  wfbdown_t wfbdown;
  struct sockaddr_in addr_out[FD_NB];
#if BOARD
  uint8_t freescan;
#else
  int8_t cptreg;
#endif // BOARD
#if RAW
  int8_t antdbm;
  uint16_t ifind;
  struct nl_sock *sk_nl;
#else
  uint8_t rawcpt;
  struct sockaddr_in addr_in;
#endif // RAW
} wfb_utils_t;

bool wfb_utils_init(uint8_t, wfb_utils_t *); 
bool wfb_utils_setfreq(int, wfb_utils_t *); 

#define DRONEID 5
#define DRONEID_IEEE 9
#define FREQID_IEEE 10

static uint8_t ieeehdr[] = {
  0x08, 0x01,                         // Frame Control : Data frame from STA to DS
  0x00, 0x00,                         // Duration
  0x66, 0x55, 0x44, 0x33, 0x22, 0x11, // Receiver MAC
  0x66, 0x55, 0x44, 0x33, 0x22, 0x11, // Transmitter MAC
  0x66, 0x55, 0x44, 0x33, 0x22, 0x11, // Destination MAC
  0x10, 0x86                          // Sequence control
};

#if RAW
/************************************************************************************************/
/*
#define RATE_CODE 0x02 // 1 M
#define RATE_CODE 0x04 //  2 M
#define RATE_CODE 0x0b //  5 M
#define RATE_CODE 0x0c //  6 M
#define RATE_CODE 0x16 // 11 M
#define RATE_CODE 0x18 // 12 M
#define RATE_CODE 0x24 // 18 M
#define RATE_CODE 0x30 // 24 M
#define RATE_CODE 0x48 // 36 M
#define RATE_CODE 0x60 // 48 M

static uint8_t radiotaphdr[] =  {
  0x00, 0x00,             // radiotap version
  0x0d, 0x00,             // radiotap header length
  0x00, 0x80, 0x08, 0x00, // radiotap present flags:  RADIOTAP_TX_FLAGS + RADIOTAP_MCS
  0x08, 0x00,             // RADIOTAP_F_TX_NOACK
  0x07, 0x00, RATE_CODE,  // MCS flags (0x07), 0x0, rate index (0x05)
};
*/
#define IEEE80211_RADIOTAP_MCS_HAVE_BW    0x01
#define IEEE80211_RADIOTAP_MCS_HAVE_MCS   0x02
#define IEEE80211_RADIOTAP_MCS_HAVE_GI    0x04
 
#define IEEE80211_RADIOTAP_MCS_HAVE_STBC  0x20

#define IEEE80211_RADIOTAP_MCS_BW_20    0
#define IEEE80211_RADIOTAP_MCS_SGI      0x04

#define IEEE80211_RADIOTAP_MCS_STBC_1  1
#define IEEE80211_RADIOTAP_MCS_STBC_SHIFT 5

#define MCS_KNOWN (IEEE80211_RADIOTAP_MCS_HAVE_MCS | IEEE80211_RADIOTAP_MCS_HAVE_BW | IEEE80211_RADIOTAP_MCS_HAVE_GI | IEEE80211_RADIOTAP_MCS_HAVE_STBC )

#define MCS_FLAGS  (IEEE80211_RADIOTAP_MCS_BW_20 | IEEE80211_RADIOTAP_MCS_SGI | (IEEE80211_RADIOTAP_MCS_STBC_1 << IEEE80211_RADIOTAP_MCS_STBC_SHIFT))

#define MCS_INDEX  0

static uint8_t radiotaphdr[] = {
    0x00, 0x00, // <-- radiotap version
    0x0d, 0x00, // <- radiotap header length
    0x00, 0x80, 0x08, 0x00, // <-- radiotap present flags:  RADIOTAP_TX_FLAGS + RADIOTAP_MCS
    0x08, 0x00,  // RADIOTAP_F_TX_NOACK
    MCS_KNOWN , MCS_FLAGS, MCS_INDEX // bitmap, flags, mcs_index
};

/************************************************************************************************/

// ONLINE_MTU on RAW receveiver should be large enought to retrieve variable size radiotap header
#define RADIOTAP_HEADER_MAX_SIZE 50
#define ONLINE_SIZE ( RADIOTAP_HEADER_MAX_SIZE + sizeof(ieeehdr) + sizeof(payhdr_t) + sizeof(subpayhdr_t) + ONLINE_MTU )
#else
#define ONLINE_SIZE ( sizeof(ieeehdr) + sizeof(payhdr_t) + sizeof(subpayhdr_t) + ONLINE_MTU )
#endif // RAW


#endif // WFB_UTILS_H
