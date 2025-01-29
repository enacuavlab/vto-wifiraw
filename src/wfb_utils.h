#ifndef WFB_UTILS_H
#define WFB_UTILS_H

#include <stdint.h>
#include <netinet/ip.h>
#include <poll.h>

#include "wfb.h"
#include "wfb_net.h"

typedef struct {
  uint32_t snd;
  uint32_t rcv;
} stream_t;

typedef struct {
  uint8_t cpt;
  int16_t chan;
  uint32_t incoming;
  uint32_t incomingsum;
  uint32_t fails;
  stream_t dev[FD_NB];
} stat_t;

typedef struct {
  int8_t raw[2];
  stat_t stat[2];
} wfb_utils_stat_t;

typedef struct {
  in_addr_t ip;
  uint16_t port;
} net_t;

typedef struct {
  uint8_t fd;
  net_t in;
  net_t out;
  struct sockaddr_in addrout;
#if RAW
  wfb_net_init_t raw;
#endif // RAW
} wfb_utils_init_t;


#if RAW
#define wfb_utils_datapos 3
#else
#define wfb_utils_datapos 1
#endif 

typedef struct {
  struct iovec head[wfb_utils_datapos + 1];
} head_t;

typedef struct {
  uint16_t feclen;
} __attribute__((packed)) wfb_utils_fec_t;

#define ONLINE_MTU PAY_MTU + sizeof(wfb_utils_fec_t)

typedef struct {
  uint8_t bufs[ ONLINE_MTU + 1];
  struct msghdr msg;
  struct iovec iovecs;
  head_t headvecs;
} wfb_utils_rawmsg_t;

typedef struct {
  uint8_t droneid;
  uint8_t msgcpt;
  uint16_t msglen;
  uint8_t seq;
  uint8_t fec;
  uint8_t num;
} __attribute__((packed)) wfb_utils_pay_t;

typedef struct {
  int16_t chan;
} __attribute__((packed)) wfb_utils_down_t;


void wfb_utils_init(wfb_utils_init_t [FD_NB], uint8_t *readcpt, uint8_t readtab[FD_NB], struct pollfd readsets[FD_NB], bool *);
void wfb_utils_presetrawmsg(wfb_utils_rawmsg_t *, ssize_t, bool );
void wfb_utils_periodic(wfb_utils_init_t *dev, bool bckup, struct iovec *downmsg[2],  wfb_utils_stat_t *pstat);

#if BOARD
#else
#include "fec.h"
#define VIDBLKSIZE 1 + FEC_N
typedef struct {
  int16_t seq;
  int16_t prevseq;
  int16_t recseq;
  uint8_t fec;
  int8_t recfec;
  int8_t k_in;
  wfb_utils_rawmsg_t *blk[ VIDBLKSIZE ];
} wfb_utils_dispatchvideo_t;
void wfb_utils_dispatchvideo(wfb_utils_init_t *sock, stream_t *pstat, wfb_utils_rawmsg_t *pmsg, wfb_utils_dispatchvideo_t *pdspvid, fec_t *fec_p);
#endif // BOARD


#endif // WFB_UTILS_H
