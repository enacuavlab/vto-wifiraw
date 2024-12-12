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
extern struct iovec wfb_net_ieeehd_vec;
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

uint16_t debugcpt = 0;

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
void wfb_utils_presetmsg(uint8_t rawmsgnb, uint8_t msgnb, uint8_t msgbuf[2][5][STORE_SIZE][ONLINE_MTU], wfb_utils_msg_t msg[2][5], ssize_t store) {

  for (uint8_t i=0; i < rawmsgnb; i++) {
    for (uint8_t j=0; j < msgnb; j++) {
      msg[i][j].len = 0;
      for (uint8_t k=0; k < store; k++) {
        msg[i][j].vecs[k].iov_base = &msgbuf[i][j][k];
        msg[i][j].vecs[k].iov_len = store;
      }
    }
  }
}

/*****************************************************************************/
void wfb_utils_presetrawmsg(wfb_utils_rawmsg_t *msg, ssize_t bufsize, bool rxflag) {

    memset(&msg->bufs, 0, sizeof(msg->bufs));
    msg->iovecs.iov_base = &msg->bufs;
    msg->iovecs.iov_len  = bufsize;
#if RAW
    if (rxflag) msg->headvecs.head[0] = wfb_net_radiotaphd_rx_vec;
    else msg->headvecs.head[0] = wfb_net_radiotaphd_tx_vec;
    msg->headvecs.head[1] = wfb_net_ieeehd_vec;
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
void wfb_utils_periodic(wfb_utils_init_t *dev, bool bckup, wfb_utils_msg_t *downmsg[2],  wfb_utils_stat_t *pstat) {
  wfb_utils_msg_t *ptrmsg;
  plog->len += sprintf((char *)plog->txt + plog->len, "%d DEBUG\n",debugcpt++);

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
  downmsg[ pstat->raw[0] ]->len = sizeof(wfb_utils_down_t);
  if (pstat->raw[1] >=0 ) downmsg[ pstat->raw[1] ]->len = sizeof(wfb_utils_down_t);
  else downmsg[ !(pstat->raw[0]) ]->len = 0;
  pstat->stat[0].incoming = 0;
  pstat->stat[1].incoming = 0;
  plog->len += sprintf((char *)plog->txt + plog->len,"Main (%d)",dev[ pstat->raw[0] + RAW0_FD].raw.freqcptcur); 
  if (pstat->raw[1] >= 0)  plog->len += sprintf((char *)plog->txt + plog->len,"  Backup (%d)",
		  dev[ (pstat->raw[1]) + RAW0_FD].raw.freqcptcur); 
  plog->txt[plog->len++] = '\n';

#else 

  if (++pstat->stat[0].cpt > 1) {
    pstat->stat[0].cpt = 0; 
    for (uint8_t raw=0; raw < 2; raw++) {
      if (pstat->stat[raw].incoming == 0) {
        if (pstat->raw[0] == raw) pstat->raw[0] = -1;
        if (pstat->raw[1] == raw) pstat->raw[1] = -1;
      }
    }
    for (uint8_t raw=0; raw < 2; raw++) {
      if (pstat->stat[raw].incoming > 0) {
        if (pstat->stat[raw].chan < 0) {
  	  pstat->raw[0] = raw;
  	  pstat->raw[1] = -1;
          plog->len += sprintf((char *)plog->txt + plog->len,"set main and unset backup\n");
        } else {
          if (pstat->stat[raw].chan >= 100) {
            pstat->stat[raw].chan -= 100;
  	    pstat->raw[0] = raw;
  	    pstat->raw[1] = !raw;
  	  } else {
  	    pstat->raw[0] = !raw;
  	    pstat->raw[1] = raw;
  	  }
  	  bool flagset=false;
          if (pstat->stat[raw].chan != dev[ !raw + RAW0_FD ].raw.freqcptcur) flagset = true;
  	  if (flagset) wfb_net_setfreq( pstat->stat[raw].chan, &dev[ !raw + RAW0_FD ].raw ); 
          plog->len += sprintf((char *)plog->txt + plog->len,"set or confirm (%d)(%d), main (%d) and backup(%d)\n",true,flagset,
			 pstat->raw[0], dev[ pstat->raw[0] + RAW0_FD ].raw.freqcptcur,
			 pstat->raw[1], dev[ pstat->raw[1] + RAW0_FD ].raw.freqcptcur);
        }
      }
    }
    for (uint8_t raw=0; raw < 2; raw++) { 
      pstat->stat[ raw ].incoming = 0;
      if (pstat->raw[raw] < 0) { 
        wfb_net_incfreq( dev[ !raw + RAW0_FD].raw.freqcptcur, &dev[ raw + RAW0_FD ].raw ); 
        plog->len += sprintf((char *)plog->txt + plog->len,"(%d)(%d) Device change chan\n",raw,dev[ raw + RAW0_FD ].raw .freqcptcur);
      }
    }
  } 
  if (pstat->raw[0] >= 0)  plog->len += sprintf((char *)plog->txt + plog->len,"Main (%d)",dev[ pstat->raw[0] + RAW0_FD].raw.freqcptcur); 
  if (pstat->raw[1] >= 0)  plog->len += sprintf((char *)plog->txt + plog->len,"  Backup (%d)",dev[ pstat->raw[1] + RAW0_FD].raw.freqcptcur); 
  plog->txt[plog->len++] = '\n';

#endif //   BOARD
#endif //   RAW
  }

  sendto(dev[ WFB_FD ].fd, plog->txt, plog->len, 0, (struct sockaddr *)&(dev[ WFB_FD ].addrout), sizeof(struct sockaddr));
  plog->len = 0;
}

