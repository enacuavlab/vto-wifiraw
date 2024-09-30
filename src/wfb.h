#ifndef WFB_H
#define WFB_H

#include <stdlib.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/if_tun.h>
#include "wfb_utils.h"

#if TELEM
#include <termios.h>
#endif // TELEM

#define TUN_MTU 1400

#define ADDR_LOCAL "127.0.0.1"
#define PORT_LOCAL_WFB	5000

/*****************************************************************************/
static void wfb_addusr(wfb_utils_t *param, uint8_t cptfdstart, uint16_t maxfd[], fd_set readset[]) {
  uint8_t dev;
  struct ifreq ifr;

  dev=WFB_FD;  // One unidirectional link
  if (-1 == (param[dev].fd = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP))) exit(-1);
  param[dev].addr_out[0].sin_family = AF_INET;
  param[dev].addr_out[0].sin_port = htons(PORT_LOCAL_WFB);
  param[dev].addr_out[0].sin_addr.s_addr = inet_addr(ADDR_LOCAL);


  dev=TUN_FD;   // One bidirectional link
  memset(&ifr, 0, sizeof(struct ifreq));
  struct sockaddr_in addr, dstaddr;
  uint16_t fd_tun_udp;
  char *addr_str_tunnel_board = "10.0.1.2";
  char *addr_str_tunnel_ground = "10.0.1.1";
#if BOARD
  strcpy(ifr.ifr_name,"airtun");
  addr.sin_addr.s_addr = inet_addr(addr_str_tunnel_board);
  dstaddr.sin_addr.s_addr = inet_addr(addr_str_tunnel_ground);
#else
  strcpy(ifr.ifr_name, "grdtun");
  addr.sin_addr.s_addr = inet_addr(addr_str_tunnel_ground);
  dstaddr.sin_addr.s_addr = inet_addr(addr_str_tunnel_board);
#endif // BOARD
  if (0 > (param[dev].fd = open("/dev/net/tun",O_RDWR))) exit(-1);
  ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
  if (ioctl( param[dev].fd, TUNSETIFF, &ifr ) < 0 ) exit(-1);
  if (-1 == (fd_tun_udp = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP))) exit(-1);
  addr.sin_family = AF_INET;
  memcpy(&ifr.ifr_addr, &addr, sizeof(struct sockaddr));
  if (ioctl( fd_tun_udp, SIOCSIFADDR, &ifr ) < 0 ) exit(-1);
  addr.sin_addr.s_addr = inet_addr( "255.255.255.0");
  memcpy(&ifr.ifr_addr, &addr, sizeof(struct sockaddr));
  if (ioctl( fd_tun_udp, SIOCSIFNETMASK, &ifr ) < 0 ) exit(-1);
  dstaddr.sin_family = AF_INET;
  memcpy(&ifr.ifr_addr, &dstaddr, sizeof(struct sockaddr));
  if (ioctl( fd_tun_udp, SIOCSIFDSTADDR, &ifr ) < 0 ) exit(-1);
  ifr.ifr_mtu = TUN_MTU;
  if (ioctl( fd_tun_udp, SIOCSIFMTU, &ifr ) < 0 ) exit(-1);
  ifr.ifr_flags = IFF_UP ;
  if (ioctl( fd_tun_udp, SIOCSIFFLAGS, &ifr ) < 0 ) exit(-1);
  for (int i=cptfdstart;i<3;i++) {
    FD_SET(param[dev].fd, &readset[i]);
    if (param[dev].fd > maxfd[i]) maxfd[i] = param[dev].fd;
  }


  dev=VID1_FD;   // Video (one unidirectional link)
  if (-1 == (param[dev].fd = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP))) exit(-1);
#if BOARD
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(5600);
  addr.sin_addr.s_addr = inet_addr(ADDR_LOCAL);
  if (-1 == bind(param[dev].fd, (struct sockaddr *)&addr, sizeof(addr))) exit(-1);
  for (int i=cptfdstart;i<3;i++) {
    FD_SET(param[dev].fd, &readset[i]);
    if (param[dev].fd > maxfd[i]) maxfd[i] = param[dev].fd;
  }
#else
  param[dev].addr_out[0].sin_family = AF_INET;
  param[dev].addr_out[0].sin_port = htons(5600);
  param[dev].addr_out[0].sin_addr.s_addr = inet_addr(ADDR_LOCAL);
#endif // BOARD
       
       
  dev=VID2_FD;   // Video (one unidirectional link)
  if (-1 == (param[dev].fd = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP))) exit(-1);
#if BOARD
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(5700);
  addr.sin_addr.s_addr = inet_addr(ADDR_LOCAL);
  if (-1 == bind(param[dev].fd, (struct sockaddr *)&addr, sizeof(addr))) exit(-1);
  for (int i=cptfdstart;i<3;i++) {
    FD_SET(param[dev].fd, &readset[i]);
    if (param[dev].fd > maxfd[i]) maxfd[i] = param[dev].fd;
  }
#else
  param[dev].addr_out[0].sin_family = AF_INET;
  param[dev].addr_out[0].sin_port = htons(5700);
  param[dev].addr_out[0].sin_addr.s_addr = inet_addr(ADDR_LOCAL);
#endif // BOARD

 
#if TELEM
  dev=TEL_FD;        // Telemetry (one bidirectional link)
#if BOARD
  if (-1 == (param[dev].fd = open(UART,O_RDWR | O_NOCTTY | O_NONBLOCK))) exit(-1);
  struct termios tty;
  if (0 != tcgetattr(param[dev].fd, &tty)) exit(-1);
  cfsetispeed(&tty,B115200);
  cfsetospeed(&tty,B115200);
  cfmakeraw(&tty);
  if (0 != tcsetattr(param[dev].fd, TCSANOW, &tty)) exit(-1);
  tcflush(param[dev].fd,TCIFLUSH);
  tcdrain(param[dev].fd);
#else            // option on ground (two directional links)
  if (-1 == (param[dev].fd = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP))) exit(-1);
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(4245);
  addr.sin_addr.s_addr = inet_addr(ADDR_LOCAL);
  if (-1 == bind(param[dev].fd, (struct sockaddr *)&addr, sizeof(addr))) exit(-1);
#endif // BOARD
  param[dev].addr_out[0].sin_family = AF_INET;
  param[dev].addr_out[0].sin_port = htons(4244);
  param[dev].addr_out[0].sin_addr.s_addr = inet_addr(ADDR_LOCAL);
  for (int i=cptfdstart;i<3;i++) {
    FD_SET(param[dev].fd, &readset[i]);
    if (param[dev].fd > maxfd[i]) maxfd[i] = param[dev].fd;
  }
#endif // TELEM
}
#endif // WFB_H
