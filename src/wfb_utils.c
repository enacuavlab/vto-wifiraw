#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>
#include <sys/timerfd.h>
#include <errno.h>
#include <stdbool.h>

#include <sys/uio.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/if_tun.h>
#include <termios.h>

#include "wfb.h"
#include "wfb_utils.h"

#define TUN_MTU 1400

#if RAW
extern struct iovec wfb_net_ieeehd_tx_vec;
extern struct iovec wfb_net_ieeehd_rx_vec;
extern struct iovec wfb_net_radiotaphd_tx_vec;
extern struct iovec wfb_net_radiotaphd_rx_vec;
#endif // RAW

typedef enum { IN, OUT, INOUT } dir_t;

typedef struct {
  uint8_t txt[1000];
  uint16_t len;
} log_t;
log_t log;
log_t *plog = &log;

wfb_utils_pay_t wfb_utils_pay;
struct iovec wfb_utils_pay_vec = { .iov_base = &wfb_utils_pay, .iov_len = sizeof(wfb_utils_pay_t)};

wfb_utils_down_t wfb_utils_down[2];


uint8_t debbuf[12][ONLINE_MTU], debidx = 0;

/*****************************************************************************/
void sock_init(wfb_utils_init_t *p, dir_t dir){
  struct sockaddr_in addr;

  if (-1 == (p->fd = socket(AF_INET, SOCK_DGRAM, 0))) exit(-1);
  
  switch(dir) {
    case IN:
    case INOUT:
      if (-1 == setsockopt(p->fd, SOL_SOCKET, SO_REUSEADDR , &(int){1}, sizeof(int))) exit(-1); 
//      if (-1 == (flags = fcntl(p->fd, F_GETFL))) exit(-1);
//      if (-1 == (fcntl(p->fd, F_SETFL, flags | O_NONBLOCK))) exit(-1);
      addr.sin_family = AF_INET;
      addr.sin_port = htons(p->in.port);
      addr.sin_addr.s_addr = p->in.ip; 
      if (-1 == bind(p->fd, &addr, sizeof(addr))) exit(-1);
      if (dir == IN) break;
      // else continue to OUT for INOUT

    case OUT:
      p->addrout.sin_family = AF_INET;
      p->addrout.sin_port = htons(p->out.port);
      p->addrout.sin_addr.s_addr = p->out.ip;
      break;

    default:
      break;
  }
}

/*****************************************************************************/
void build_tun(uint8_t *fd) {
  uint16_t fd_tun_udp;
  struct ifreq ifr;

  memset(&ifr, 0, sizeof(struct ifreq));
  struct sockaddr_in addr, dstaddr;
#if BOARD
  strcpy(ifr.ifr_name,"airtun");
  addr.sin_addr.s_addr = inet_addr(TUNIP_BOARD);
  dstaddr.sin_addr.s_addr = inet_addr(TUNIP_GROUND);
#else
  strcpy(ifr.ifr_name, "grdtun");
  addr.sin_addr.s_addr = inet_addr(TUNIP_GROUND);
  dstaddr.sin_addr.s_addr = inet_addr(TUNIP_BOARD);
#endif // BOARD
  if (0 > (*fd = open("/dev/net/tun",O_RDWR))) exit(-1);
  ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
  if (ioctl(*fd, TUNSETIFF, &ifr ) < 0 ) exit(-1);
  if (-1 == (fd_tun_udp = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP))) exit(-1);
  addr.sin_family = AF_INET;
  memcpy(&ifr.ifr_addr, &addr, sizeof(struct sockaddr));
  if (ioctl( fd_tun_udp, SIOCSIFADDR, &ifr ) < 0 ) exit(-1);
  addr.sin_addr.s_addr = inet_addr(IPBROAD);
  memcpy(&ifr.ifr_addr, &addr, sizeof(struct sockaddr));
  if (ioctl( fd_tun_udp, SIOCSIFNETMASK, &ifr ) < 0 ) exit(-1);
  dstaddr.sin_family = AF_INET;
  memcpy(&ifr.ifr_addr, &dstaddr, sizeof(struct sockaddr));
  if (ioctl( fd_tun_udp, SIOCSIFDSTADDR, &ifr ) < 0 ) exit(-1);
  ifr.ifr_mtu = TUN_MTU;
  if (ioctl( fd_tun_udp, SIOCSIFMTU, &ifr ) < 0 ) exit(-1);
  ifr.ifr_flags = IFF_UP ;
  if (ioctl( fd_tun_udp, SIOCSIFFLAGS, &ifr ) < 0 ) exit(-1);
}

/*****************************************************************************/
#if TELEM
#if BOARD
void build_uart(uint8_t *fd) {
  if (-1 == (*fd = open(UART,O_RDWR | O_NOCTTY | O_NONBLOCK))) exit(-1);
  struct termios tty;
  if (0 != tcgetattr(*fd, &tty)) exit(-1);
  cfsetispeed(&tty,B115200);
  cfsetospeed(&tty,B115200);
  cfmakeraw(&tty);
  if (0 != tcsetattr(*fd, TCSANOW, &tty)) exit(-1);
  tcflush(*fd,TCIFLUSH);
  tcdrain(*fd);
}
#endif // BOARD
#endif // TELEM
       
/*****************************************************************************/
void wfb_utils_init(wfb_utils_init_t dev[FD_NB], uint8_t *readcpt, uint8_t readtab[FD_NB], struct pollfd readsets[FD_NB], 
  bool *bckup)
{
  uint8_t devcpt = 0;
  *bckup = false;

  devcpt = TIME_FD;
  dev[devcpt].fd = timerfd_create(CLOCK_MONOTONIC, 0);
  readsets[*readcpt].fd = dev[devcpt].fd;
  readsets[*readcpt].events = POLLIN;
  struct itimerspec period = { { PERIOD_DELAY_S, 0 }, { PERIOD_DELAY_S, 0 } };
  timerfd_settime(dev[devcpt].fd, 0, &period, NULL);
  readtab[*readcpt] = devcpt;
  (*readcpt)++;

#if RAW
  uint8_t tmpcpt=(*readcpt);
  for (devcpt = RAW0_FD; devcpt <= RAW1_FD; devcpt++) {
    if (wfb_net_init(&dev[devcpt].raw)) {
      if (INITCHAN[devcpt - RAW0_FD] < dev[devcpt].raw.freqsnb) {
        if (wfb_net_setfreq( INITCHAN[devcpt - RAW0_FD] , &dev[devcpt].raw )) {
          printf("DEV [%s][%d]\n", dev[devcpt].raw.name,INITCHAN[devcpt - RAW0_FD]);
          dev[devcpt].fd = dev[devcpt].raw.fd;
          readtab[*readcpt] = devcpt;
          readsets[*readcpt].fd = dev[devcpt].fd;
          readsets[*readcpt].events = POLLIN;
          (*readcpt)++;
	} else if (devcpt == RAW0_FD) exit(-1);
      } else if (devcpt == RAW0_FD) exit(-1);
    } else if (devcpt == RAW0_FD) exit(-1);
  }
  if ((2+tmpcpt) == (*readcpt)) *bckup = true;

#else // RAW 
  for (devcpt = RAW0_FD; devcpt <= RAW1_FD; devcpt++) {
    dev[devcpt].in.port = PORTS_RAW[ devcpt - RAW0_FD ];
    dev[devcpt].out.port = PORTS_RAW[ devcpt - RAW0_FD ];
    dev[devcpt].in.ip = inet_addr(ADDR_LOCAL_RAW);
    dev[devcpt].out.ip = inet_addr(ADDR_REMOTE_RAW);
    sock_init(&dev[devcpt],INOUT);
    readtab[*readcpt] = devcpt;
    readsets[*readcpt].fd = dev[devcpt].fd;
    readsets[*readcpt].events = POLLIN;
    (*readcpt)++;
  }
#endif // RAW

  devcpt = WFB_FD;
  dev[devcpt].out.port = WFBPORT;
  dev[devcpt].out.ip = inet_addr(ADDR_LOCAL);
  sock_init(&dev[devcpt],OUT);

  devcpt = TUN_FD; // One bidirectional link
  build_tun(&dev[devcpt].fd);
  readtab[*readcpt] = devcpt;
  readsets[*readcpt].fd = dev[devcpt].fd;
  readsets[*readcpt].events = POLLIN;
  (*readcpt)++;

  for (devcpt = VID1_FD; devcpt <= VID2_FD; devcpt++) {
#if BOARD  
    readtab[*readcpt] = devcpt;
    dev[devcpt].in.port = PORTS_VID[ devcpt - VID1_FD ];
    dev[devcpt].in.ip = inet_addr(ADDR_LOCAL);
    sock_init(&dev[devcpt],IN);
    readsets[*readcpt].fd = dev[devcpt].fd;
    readsets[*readcpt].events = POLLIN;
    (*readcpt)++;
#else
    dev[devcpt].out.port = PORTS_VID[ devcpt - VID1_FD ];
    dev[devcpt].out.ip = inet_addr(ADDR_LOCAL);
    sock_init(&dev[devcpt],OUT);
#endif // BOARD
  }    

#if TELEM
  devcpt = TEL_FD;
#if BOARD
  dev[devcpt].out.port = TELPORTDOWN;
  dev[devcpt].out.ip = inet_addr(ADDR_LOCAL);
  dev[devcpt].addrout.sin_family = AF_INET;
  build_uart(&dev[devcpt].fd);
#else
  dev[devcpt].in.port = TELPORTUP;
  dev[devcpt].in.ip = inet_addr(ADDR_LOCAL);
  dev[devcpt].out.port = TELPORTDOWN;
  dev[devcpt].out.ip = inet_addr(ADDR_LOCAL);
  sock_init(&dev[devcpt],INOUT);
#endif // BOARD
  readtab[*readcpt] = devcpt;
  readsets[*readcpt].fd = dev[devcpt].fd;
  readsets[*readcpt].events = POLLIN;
  (*readcpt)++;
#endif // TELEM
}

/*****************************************************************************/
void wfb_utils_presetrawmsg(wfb_utils_rawmsg_t *msg, ssize_t bufsize, bool rxflag) {

    memset(&msg->bufs, 0, sizeof(msg->bufs));
    msg->iovecs.iov_base = &msg->bufs;
    msg->iovecs.iov_len  = bufsize;
#if RAW
    if (rxflag) {
      msg->headvecs.head[0] = wfb_net_radiotaphd_rx_vec;
      msg->headvecs.head[1] = wfb_net_ieeehd_rx_vec;
      memset(wfb_net_ieeehd_rx_vec.iov_base, 0, wfb_net_ieeehd_rx_vec.iov_len);
    } else {
      msg->headvecs.head[0] = wfb_net_radiotaphd_tx_vec;
      msg->headvecs.head[1] = wfb_net_ieeehd_tx_vec;
    }
    memset(wfb_utils_pay_vec.iov_base, 0, wfb_utils_pay_vec.iov_len);
    msg->headvecs.head[2] = wfb_utils_pay_vec;
    msg->headvecs.head[3] = msg->iovecs;
    msg->msg.msg_iovlen = 4;
#else
    msg->headvecs.head[0] = wfb_utils_pay_vec;
    msg->headvecs.head[1] = msg->iovecs;
    msg->msg.msg_iovlen = 2;
#endif // RAW
    msg->msg.msg_iov = &msg->headvecs.head[0];
}

/*****************************************************************************/
void wfb_utils_periodic(wfb_utils_init_t *dev, bool bckup, struct iovec *downmsg[2],  wfb_utils_stat_t *pstat) {
  uint8_t template[]="(%d)(%d) devraw(%d) fails(%d) incom(%d) NbBytes(snd/rcv)\
  [%d](%d)(%d) [%d](%d)(%d) [%d](%d)(%d) [%d](%d)(%d) [%d](%d)(%d)\n";
  plog->len += sprintf((char *)plog->txt + plog->len, (char *)template, 
                 pstat->raw[0],pstat->raw[1],
		 0, pstat->stat[0].fails, pstat->stat[0].incoming,
                 WFB_FD,pstat->stat[0].dev[WFB_FD].snd, pstat->stat[0].dev[WFB_FD].rcv,
                 TUN_FD,pstat->stat[0].dev[TUN_FD].snd, pstat->stat[0].dev[TUN_FD].rcv,
                 VID1_FD,pstat->stat[0].dev[VID1_FD].snd, pstat->stat[0].dev[VID1_FD].rcv,
                 VID2_FD,pstat->stat[0].dev[VID2_FD].snd, pstat->stat[0].dev[VID2_FD].rcv,
                 TEL_FD,pstat->stat[0].dev[TEL_FD].snd, pstat->stat[0].dev[TEL_FD].rcv);
  memset(&pstat->stat[0].dev, 0, sizeof(pstat->stat[0].dev));
  if (bckup) {
#if RAW
    plog->len += sprintf((char *)plog->txt + plog->len, (char *)template, 
                 pstat->raw[0],pstat->raw[1],
		 1, pstat->stat[1].fails, pstat->stat[1].incoming,
                 WFB_FD,pstat->stat[1].dev[WFB_FD].snd, pstat->stat[1].dev[WFB_FD].rcv,
                 TUN_FD,pstat->stat[1].dev[TUN_FD].snd, pstat->stat[1].dev[TUN_FD].rcv,
                 VID1_FD,pstat->stat[1].dev[VID1_FD].snd, pstat->stat[1].dev[VID1_FD].rcv,
                 VID2_FD,pstat->stat[1].dev[VID2_FD].snd, pstat->stat[1].dev[VID2_FD].rcv,
                 TEL_FD,pstat->stat[1].dev[TEL_FD].snd, pstat->stat[1].dev[TEL_FD].rcv);
    memset(&pstat->stat[1].dev, 0, sizeof(pstat->stat[1].dev));

#if BOARD
  struct iovec *ptrmsg;
  if (pstat->raw[1] >= 0) {	  
    if (pstat->stat[ pstat->raw[0] ].incoming > 0) {
      if (pstat->stat[ pstat->raw[0] ].incoming > pstat->stat[ pstat->raw[1] ].incoming) {
	ptrmsg = downmsg[ pstat->raw[0] ];
        downmsg[ pstat->raw[0] ] = downmsg[ pstat->raw[1] ]; 
        downmsg[ pstat->raw[1] ] = ptrmsg;
        pstat->raw[0] = pstat->raw[1];
        wfb_utils_down[ !(pstat->raw[0]) ].chan = -1;
        pstat->raw[1] = -1;
        plog->len += sprintf((char *)plog->txt + plog->len,"[ Main switch with backup ]\n");
      }
    } else if (pstat->stat[pstat->raw[1]].incoming > 0) pstat->raw[1] = -1;
  } else {
    if (pstat->stat[ !(pstat->raw[0]) ].incoming == 0) {
      if (pstat->raw[0] == 0) {
        wfb_utils_down[ !(pstat->raw[0]) ].chan = dev[ pstat->raw[0] + RAW0_FD].raw.freqcptcur;
        wfb_utils_down[ pstat->raw[0] ].chan = 100 + dev[ !(pstat->raw[0]) + RAW0_FD].raw.freqcptcur;
      } else {
        wfb_utils_down[ !(pstat->raw[0]) ].chan = 100 + dev[ !(pstat->raw[0]) + RAW0_FD].raw.freqcptcur;
        wfb_utils_down[ pstat->raw[0] ].chan = dev[ pstat->raw[0] + RAW0_FD].raw.freqcptcur;
      }
      pstat->raw[1] = !(pstat->raw[0]);
      plog->len += sprintf((char *)plog->txt + plog->len,"Backup set chan (%d)\n",dev[ (pstat->raw[1]) + RAW0_FD].raw.freqcptcur);
    }
  }
  if (pstat->raw[1] < 0) {
    wfb_net_incfreq( dev[ pstat->raw[0] + RAW0_FD].raw.freqcptcur, &dev[ !(pstat->raw[0]) + RAW0_FD ].raw ); 
    wfb_utils_down[ pstat->raw[0] ].chan = -1;
    wfb_utils_down[ !(pstat->raw[0]) ].chan = -1;	  
    plog->len += sprintf((char *)plog->txt + plog->len,"Backup change chan (%d)\n",dev[ !(pstat->raw[0]) + RAW0_FD].raw.freqcptcur); 
  }
  downmsg[ pstat->raw[0] ]->iov_len = sizeof(wfb_utils_down_t);
  if (pstat->raw[1] >=0 ) downmsg[ pstat->raw[1] ]->iov_len = sizeof(wfb_utils_down_t);
  else downmsg[ !(pstat->raw[0]) ]->iov_len = 0;
  pstat->stat[0].incoming = 0;
  pstat->stat[1].incoming = 0;
  plog->len += sprintf((char *)plog->txt + plog->len,"Main dev(%d) chan(%d)",pstat->raw[0],dev[ pstat->raw[0] + RAW0_FD].raw.freqcptcur); 
  if (pstat->raw[1] >= 0)  plog->len += sprintf((char *)plog->txt + plog->len,"  Backup dev(%d) chan(%d)", pstat->raw[1],
		  dev[ (pstat->raw[1]) + RAW0_FD].raw.freqcptcur); 
  plog->txt[plog->len++] = '\n';plog->txt[plog->len++] = '\n';

#else 
  bool flag = false;
  if (++pstat->stat[0].cpt > 1) {
    pstat->stat[0].cpt = 0; 

    for (uint8_t raw=0; raw < 2; raw++) {
      if (pstat->stat[raw].incoming == 0) {
        if (pstat->raw[0] == raw) pstat->raw[0] = -1;
        if (pstat->raw[1] == raw) pstat->raw[1] = -1;
      }
    }

    for (uint8_t raw=0; raw < 2; raw++) {
      if ((pstat->stat[raw].incoming > 0) && (pstat->stat[!(raw)].incoming == 0)) {
        if (pstat->stat[raw].chan < 0) {
          pstat->raw[0] = raw; pstat->raw[1] = -1;
          plog->len += sprintf((char *)plog->txt + plog->len,"main set dev(%d)\n",pstat->raw[0]);
	} else {
          if (pstat->stat[raw].chan >= 100) {
            pstat->stat[raw].chan -= 100;
	    pstat->raw[0] = raw; pstat->raw[1] = !(raw); 
  	    wfb_net_setfreq( pstat->stat[pstat->raw[0]].chan, &dev[ pstat->raw[1] + RAW0_FD ].raw);
            plog->len += sprintf((char *)plog->txt + plog->len,"main set, backup set dev[%d] chan(%d)\n", pstat->raw[1],
                               dev[ pstat->raw[1] + RAW0_FD ].raw.freqcptcur);
	  } else {
	    pstat->raw[0] = !(raw); pstat->raw[1] = raw; 
  	    wfb_net_setfreq( pstat->stat[pstat->raw[0]].chan, &dev[ pstat->raw[1] + RAW0_FD ].raw);
            plog->len += sprintf((char *)plog->txt + plog->len,"main switch, backup set dev[%d] chan(%d)\n", pstat->raw[1],
                               dev[ pstat->raw[1] + RAW0_FD ].raw.freqcptcur);
	  }
	}
      }
    }

    pstat->stat[0].incoming = 0; pstat->stat[1].incoming = 0; 
    if (pstat->raw[0] < 0) {
      wfb_net_incfreq( dev[ 1 + RAW0_FD].raw.freqcptcur, &dev[ 0 + RAW0_FD ].raw ); 
      wfb_net_incfreq( dev[ 0 + RAW0_FD].raw.freqcptcur, &dev[ 1 + RAW0_FD ].raw ); 
      plog->len += sprintf((char *)plog->txt + plog->len,"devices increment chan (%d (%d)\n",
			dev[ 0 + RAW0_FD ].raw .freqcptcur, dev[ 1 + RAW0_FD ].raw .freqcptcur);
    }
  }

  if (pstat->raw[0] >= 0)  plog->len += sprintf((char *)plog->txt + plog->len,"Main dev(%d) chan(%d)",pstat->raw[0],dev[ pstat->raw[0] + RAW0_FD].raw.freqcptcur); 
  if (pstat->raw[1] >= 0)  plog->len += sprintf((char *)plog->txt + plog->len,"  Backup dev(%d) chan(%d)",pstat->raw[1],dev[ pstat->raw[1] + RAW0_FD].raw.freqcptcur); 
  plog->txt[plog->len++] = '\n';plog->txt[plog->len++] = '\n';

#endif //   BOARD
#endif //   RAW
  }

  sendto(dev[ WFB_FD ].fd, plog->txt, plog->len, 0, (struct sockaddr *)&(dev[ WFB_FD ].addrout), sizeof(struct sockaddr));
  plog->len = 0;
}

#if BOARD
#else
/*****************************************************************************/
void wfb_utils_dispatchvideo(wfb_utils_init_t *sock, stream_t *pstat, wfb_utils_rawmsg_t *pmsg, wfb_utils_dispatchvideo_t *pdspvid, fec_t *fec_p) {
  ssize_t len = 0;
  bool nextseq=false, display=false,reset=false;
  uint8_t k_out = 0, idx = 0, outblocksbuf[FEC_N-FEC_K][ONLINE_MTU];
  uint8_t *pdebug;


  // DEBUG
/*
  if (rand() < (0.1 * ((double)RAND_MAX + 1.0))) { 
    memset(debbuf[debidx], 0, ONLINE_MTU);
    memcpy(debbuf[debidx], (uint8_t *)pmsg->headvecs.head[wfb_utils_datapos].iov_base, pmsg->headvecs.head[wfb_utils_datapos].iov_len);
    memset((uint8_t *)pmsg->headvecs.head[wfb_utils_datapos].iov_base, 0, pmsg->headvecs.head[wfb_utils_datapos].iov_len);
    printf("\nCleared\n");
    fflush(stdout);
    return(0);
  } 
  if (wfb_utils_pay.fec == 1) return(0);
  if (wfb_utils_pay.fec == 3) return(0);
  if (wfb_utils_pay.fec == 5) return(0);
  if (wfb_utils_pay.fec == 7) return(0);
*/


  if (pdspvid->seq < 0) {pdspvid->prevseq = 0; pdspvid->seq = wfb_utils_pay.seq; pdspvid->fec = wfb_utils_pay.fec; }
  else  {
    uint8_t dum = (1 + pdspvid->fec); if (dum == FEC_N) pdspvid->fec = 0;  else pdspvid->fec = dum;
    if (pdspvid->fec == 0) {
      dum = (1 + pdspvid->seq); if (dum == 255) pdspvid->seq = 0;  else pdspvid->seq = dum;
    }
    if ((pdspvid->recfec < 0) && ((pdspvid->seq != wfb_utils_pay.seq) || (pdspvid->fec != wfb_utils_pay.fec))) {
      pdspvid->recseq = pdspvid->seq; pdspvid->recfec = pdspvid->fec;
    }
    pdspvid->seq = wfb_utils_pay.seq; pdspvid->fec = wfb_utils_pay.fec;

    if (pdspvid->prevseq != wfb_utils_pay.seq) { pdspvid->prevseq = wfb_utils_pay.seq; nextseq=true; }
  }
  pmsg->headvecs.head[wfb_utils_datapos].iov_len = wfb_utils_pay.msglen;
/*
  pdebug = (uint8_t *)pmsg->headvecs.head[wfb_utils_datapos].iov_base;
  printf("\nreadmsg recorded seq[%d)  num[%d] seq(%d) fec(%d) %02X[%ld]%02X\n",pdspvid->seq,
	  wfb_utils_pay.num,wfb_utils_pay.seq,wfb_utils_pay.fec,
          *(16 + pdebug),
          pmsg->headvecs.head[wfb_utils_datapos].iov_len,
          *(pmsg->headvecs.head[wfb_utils_datapos].iov_len + pdebug - 1));
*/
  uint8_t *outblocks[FEC_N-FEC_K];
  if (!nextseq) {
    pdspvid->blk[wfb_utils_pay.fec] = pmsg;
    if (wfb_utils_pay.fec < FEC_K) { 
      if (pdspvid->recfec < 0) {
        pdspvid->k_in = wfb_utils_pay.fec; k_out = (wfb_utils_pay.fec + 1);
        display = true;
      } else {
	display = false;
      }
    }
  } else {
    if (pdspvid->recfec < 0) {
      display = true; reset = true;
      pdspvid->k_in = FEC_N;
      pdspvid->blk[FEC_N] = pmsg;
      k_out = 1 + FEC_N;
    } else {
      if (! ((0 <= pdspvid->recfec ) && ( pdspvid->recfec < FEC_K))) { 
        reset = true;  display = true;  
        pdspvid->k_in = FEC_N;
        pdspvid->blk[FEC_N] = pmsg;
        k_out = 1 + FEC_N;
      } else {
        if ((pdspvid->recfec == 0) && (pdspvid->recseq == wfb_utils_pay.seq)) reset = true;
	else {
          display = true; reset = true;
          unsigned index[FEC_K];
          uint8_t *inblocks[FEC_K];
          uint8_t  alldata=0;
          uint8_t j=FEC_K;
          idx = 0;
          for (uint8_t k=0;k<FEC_K;k++) {
            index[k] = 0;
            inblocks[k] = (uint8_t *)0;
            if (k < (FEC_N - FEC_K)) outblocks[k] = (uint8_t *)0;
            if ( pdspvid->blk[k] ) {
              inblocks[k] = (uint8_t *)pdspvid->blk[k]->headvecs.head[wfb_utils_datapos].iov_base;
              index[k] = k;
              alldata |= (1 << k);
            } else {
              for(;j < FEC_N; j++) {
                if ( pdspvid->blk[j] ) {
                  inblocks[k] = (uint8_t *)pdspvid->blk[j]->headvecs.head[wfb_utils_datapos].iov_base;
                  outblocks[idx] = &outblocksbuf[idx][0]; idx++;
                  index[k] = j;
                  j++;
      	          alldata |= (1 << k);
      	          break;
      	        }
      	      }
            }
          }
/*
          printf("seq(%d) alldata (%d) idx(%d)\n",wfb_utils_pay.seq,alldata,idx);
*/
          if ((alldata == 255)&&(idx > 0)&&(idx <= (FEC_N - FEC_K))) {
            pdspvid->k_in = pdspvid->recfec;
/*
            for (uint8_t k=0;k<FEC_K;k++) printf("%d ",index[k]);
            printf("\nDECODE (%d)\n",idx);
*/
            fec_decode(fec_p,
              (const gf*restrict const*restrict const)inblocks,
              (gf*restrict const*restrict const)outblocks,
              (const unsigned*restrict const)index, 
              ONLINE_MTU);
	    if (wfb_utils_pay.fec == 0) {
              pdspvid->blk[FEC_K] = pmsg;
              k_out = 1 + FEC_K;
	    } else k_out = FEC_K;
	  } else {
            outblocks[0] = (uint8_t *)0;
            pdspvid->k_in = 1 + pdspvid->recfec;
            pdspvid->blk[FEC_K] = pmsg;
            k_out = 1 + FEC_K;
	  }
	}
      }
    }
  }

  if (display) {
    wfb_utils_rawmsg_t *prawmsg;
    uint8_t *sdbuf;
    uint16_t sdlen;
    idx=0;
    for (uint8_t k = pdspvid->k_in; k < k_out; k++) {
      sdbuf = 0;
      if (pdspvid->recfec < 0) {
        sdbuf = (uint8_t *)(pmsg->headvecs.head[wfb_utils_datapos].iov_base) + sizeof(wfb_utils_fec_t); 
        sdlen = pmsg->headvecs.head[wfb_utils_datapos].iov_len - sizeof(wfb_utils_fec_t); 
      } else{
        prawmsg = pdspvid->blk[k];
        if (prawmsg) {
          sdbuf = (uint8_t *)(prawmsg->headvecs.head[wfb_utils_datapos].iov_base) + sizeof(wfb_utils_fec_t); 
          sdlen = (prawmsg->headvecs.head[wfb_utils_datapos].iov_len) - sizeof(wfb_utils_fec_t); 
        } else {
          if (outblocks[idx]) {
            sdlen = ((wfb_utils_fec_t *)&outblocksbuf[idx][0])->feclen;
            sdbuf = &outblocksbuf[idx][sizeof(wfb_utils_fec_t)];
	    idx++;
	  }
        }
      }
      if(sdbuf) {
        if ((len = sendto(sock->fd, sdbuf, sdlen, MSG_DONTWAIT, (struct sockaddr *)&(sock->addrout), sizeof(struct sockaddr))) > 0) {
          pstat->rcv += len;
/*
	  pdebug = sdbuf;
          printf("%02X[%ld][%d]%02X ",*(14 + pdebug),
              len,
              sdlen,
              *(sdlen + pdebug - 1)); fflush(stdout);
*/
        }
      }
    }
 //   printf("\n");
  }

  if (reset) {
/*
    printf("\nRESET backup(%d)\n",wfb_utils_pay.fec);
*/
    memset(pdspvid->blk, 0, VIDBLKSIZE * sizeof(wfb_utils_rawmsg_t *)); 
    pdspvid->blk[wfb_utils_pay.fec] = pmsg;
    if (( pdspvid->recfec >= 0) && (wfb_utils_pay.fec != 0)) { pdspvid->recseq = wfb_utils_pay.seq; pdspvid->recfec = 0; }
    else if (! ((pdspvid->recfec == 0) && (pdspvid->recseq == wfb_utils_pay.seq))) pdspvid->recfec = -1;
  }
}
#endif // BOARD
