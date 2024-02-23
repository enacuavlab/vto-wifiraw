#ifndef WFB_H
#define WFB_H

#define ONLINE_MTU 1450

typedef struct {
  uint16_t seq;
  uint16_t len;  // len for the full payload not including (pay_hdr_t)
  uint64_t stp_n;
} __attribute__((packed)) payhdr_t;

typedef struct {
  uint8_t id;
  uint16_t len; // len for the subpayload not including (sub_pay_hdr_t)
} __attribute__((packed)) subpayhdr_t;


typedef enum { RAW0_FD, RAW1_FD, WFB_FD, TUN_FD, VID1_FD, FD_NB } cannal_t;

typedef struct {
  char name[20];
  fd_set readset_raw[3];
  fd_set readset_all[3];
  uint16_t maxfd_raw[3];
  uint16_t maxfd_all[3];
  uint16_t fd[FD_NB];
  int8_t  offsetraw;
  struct sockaddr_in addr_out[FD_NB];
} init_t;

#include <linux/if_tun.h>
#define TUN_MTU 1400

#ifdef RAW
#include <net/ethernet.h>
#include <linux/filter.h>
#include <linux/if_packet.h>

static uint8_t radiotaphdr[] =  {
  0x00, 0x00,             // radiotap version
  0x0d, 0x00,             // radiotap header length
  0x00, 0x80, 0x08, 0x00, // radiotap present flags:  RADIOTAP_TX_FLAGS + RADIOTAP_MCS
  0x08, 0x00,             // RADIOTAP_F_TX_NOACK
  0x07, 0x00, 0x02,       // MCS flags (0x07), 0x0, rate index (0x05)
};
static uint8_t ieeehdr[] = {
  0x08, 0x01,                         // Frame Control : Data frame from STA to DS
  0x00, 0x00,                         // Duration
  0x66, 0x55, 0x44, 0x33, 0x22, 0x11, // Receiver MAC 
  0x66, 0x55, 0x44, 0x33, 0x22, 0x11, // Transmitter MAC
  0x66, 0x55, 0x44, 0x33, 0x22, 0x11, // Destination MAC
  0x10, 0x86                          // Sequence control
};
static uint8_t droneidbuf[] =  {
 0x00,0x00,0x09,0x00,0x02,0x00,0x00,0x00,0x10,0x80,0x00,0x00,0x00,0xff,0xff,0xff,
 0xff,0xff,0xff,0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x64,0x00,0x11,0x10,0x00,0x09,0x4d,
 0x59,0x54,0x45,0x53,0x54,0x4e,0x45,0x54,0xdd,0x26,0x6a,0x5c,0x35,0x20,0x20,0x20,
 0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
 0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
 0x19,0x45,0xfb,0x1c
};
#define DRONIDSSIDPOS_TX 47
#define DRONIDSSIDPOS_RX 70

uint32_t crc32_table[256];
void build_crc32_table(void) {
  for(uint32_t i=0;i<256;i++) {
    uint32_t ch=i;
    uint32_t crc=0;
    for(uint32_t j=0;j<8;j++) {
      uint32_t b=(ch^crc)&1;
      crc>>=1;
      if(b) crc=crc^0xEDB88320;
      ch>>=1;
    }
    crc32_table[i]=crc;
  }
}
// ONLINE_MTU on RAW receveiver should be large enought to retrieve variable size radiotap header
#define RADIOTAP_HEADER_MAX_SIZE 50
#define ONLINE_SIZE ( RADIOTAP_HEADER_MAX_SIZE + sizeof(ieeehdr) + sizeof(payhdr_t) + sizeof(subpayhdr_t) + ONLINE_MTU )
#define DRONEID 5
#define DRONEID_IEEE 9
#define ADDR_LOCAL "127.0.0.1"
#else
#define ONLINE_SIZE ( sizeof(payhdr_t) + sizeof(subpayhdr_t) + ONLINE_MTU )
#endif // RAW

/*****************************************************************************/
bool init_raw(char *name,uint16_t noraw_udp_port, uint8_t dev, init_t *param) {
  bool ret=true;
  strcpy(param->name,name);
#ifdef RAW
  uint8_t flags;
  uint16_t protocol = htons(ETH_P_ALL);
  if (-1 == (param->fd[dev] = socket(AF_PACKET,SOCK_RAW,protocol))) exit(-1);
  if (-1 == (flags = fcntl(param->fd[dev], F_GETFL))) exit(-1);
  if (-1 == (fcntl(param->fd[dev], F_SETFL, flags | O_NONBLOCK))) exit(-1);
  struct sock_filter zero_bytecode = BPF_STMT(BPF_RET | BPF_K, 0);
  struct sock_fprog zero_program = { 1, &zero_bytecode};
  if (-1 == setsockopt(param->fd[dev], SOL_SOCKET, SO_ATTACH_FILTER, &zero_program, sizeof(zero_program))) exit(-1);
  struct ifreq ifr;
  memset(&ifr, 0, sizeof(struct ifreq));
  strncpy( ifr.ifr_name, name, sizeof( ifr.ifr_name ) - 1 );
  if (ioctl( param->fd[dev], SIOCGIFINDEX, &ifr ) < 0 ) exit(-1);
  struct sockaddr_ll sll;
  memset( &sll, 0, sizeof( sll ) );
  sll.sll_family   = AF_PACKET;
  sll.sll_ifindex  = ifr.ifr_ifindex;
  sll.sll_protocol = protocol;
  if (-1 == bind(param->fd[dev], (struct sockaddr *)&sll, sizeof(sll))) exit(-1);
  char drain[1];
  while (recv(param->fd[dev], drain, sizeof(drain), MSG_DONTWAIT) >= 0) {
    printf("----\n");
  };
  struct sock_filter full_bytecode = BPF_STMT(BPF_RET | BPF_K, (u_int)-1);
  struct sock_fprog full_program = { 1, &full_bytecode};
  if (-1 == setsockopt(param->fd[dev], SOL_SOCKET, SO_ATTACH_FILTER, &full_program, sizeof(full_program))) ret=false;
  build_crc32_table();
  param->offsetraw=sizeof(radiotaphdr)+sizeof(ieeehdr);
  ieeehdr[DRONEID_IEEE] = DRONEID;
#else //  RAW

  struct sockaddr_in addr_in[FD_NB];
  #if ROLE
  addr_in[dev].sin_addr.s_addr = inet_addr(ADDR_REMOTE_BOARD);
  param->addr_out[dev].sin_addr.s_addr = inet_addr(ADDR_REMOTE_GROUND);
  #else //  ROLE
  addr_in[dev].sin_addr.s_addr = inet_addr(ADDR_REMOTE_GROUND);
  param->addr_out[dev].sin_addr.s_addr = inet_addr(ADDR_REMOTE_BOARD);
  #endif // ROLE

  if (-1 == (param->fd[dev]=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP))) exit(-1);
  addr_in[dev].sin_family = AF_INET;
  addr_in[dev].sin_port = htons(noraw_udp_port);
  if (-1 == bind(param->fd[dev], (struct sockaddr *)&addr_in[dev], sizeof(addr_in))) exit(-1);
  param->addr_out[dev].sin_family = AF_INET;
  param->addr_out[dev].sin_port = htons(noraw_udp_port);
  param->offsetraw=0;
#endif // RAW

  return(ret);
}

/*****************************************************************************/
bool wfb_init(char *raw0_name, char *raw1_name, init_t *param) {
  bool ret=false;
  struct sockaddr_in addr_in[FD_NB];
  struct ifreq ifr;
  uint16_t dev;

  FD_ZERO(&(param->readset_raw[0]));FD_ZERO(&(param->readset_raw[1]));FD_ZERO(&(param->readset_raw[2]));
  FD_ZERO(&(param->readset_all[0]));FD_ZERO(&(param->readset_all[1]));FD_ZERO(&(param->readset_all[2]));
  memset(&(param->fd),0,FD_NB*sizeof(uint16_t));

  if ((*raw1_name) != '\000') { 
    if (init_raw(raw1_name, 5100,RAW1_FD,param)) {
      FD_SET(param->fd[RAW1_FD], &(param->readset_raw[1]));param->maxfd_raw[1]=param->fd[RAW1_FD];
      FD_SET(param->fd[RAW1_FD], &(param->readset_raw[2]));param->maxfd_raw[2]=param->fd[RAW1_FD];
      FD_SET(param->fd[RAW1_FD], &(param->readset_all[1]));param->maxfd_all[1]=param->fd[RAW1_FD];
      FD_SET(param->fd[RAW1_FD], &(param->readset_all[2]));param->maxfd_all[2]=param->fd[RAW1_FD];
      ret=true;
      if ((*raw0_name) != '\000') {
        if (init_raw(raw0_name,5150,RAW0_FD,param)) {
          FD_SET(param->fd[RAW0_FD], &(param->readset_raw[0]));param->maxfd_raw[0]=param->fd[RAW0_FD];
          FD_SET(param->fd[RAW0_FD], &(param->readset_raw[2]));if (param->maxfd_raw[2] < param->fd[RAW0_FD]) param->maxfd_raw[2]=param->fd[RAW0_FD];
          FD_SET(param->fd[RAW0_FD], &(param->readset_all[0]));if (param->maxfd_all[0] < param->fd[RAW0_FD]) param->maxfd_all[0]=param->fd[RAW0_FD];
          FD_SET(param->fd[RAW0_FD], &(param->readset_all[2]));if (param->maxfd_all[2] < param->fd[RAW0_FD]) param->maxfd_all[2]=param->fd[RAW0_FD];
        } else ret=false;
      }
    }
  }

  dev=WFB_FD;   // One unidirectional wfb telemetry
#if ROLE
#else
  if (-1 == (param->fd[dev]=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP))) exit(-1);
  param->addr_out[dev].sin_family = AF_INET;
  param->addr_out[dev].sin_port = htons(5000);
  param->addr_out[dev].sin_addr.s_addr = inet_addr(ADDR_LOCAL);
#endif // ROLE


  dev=TUN_FD;   // Tunnel (one bidirectional link)
  memset(&ifr, 0, sizeof(struct ifreq));
  struct sockaddr_in addr, dstaddr;
  uint16_t fd_tun_udp;
  char *addr_str_tunnel_board = "10.0.1.2";
  char *addr_str_tunnel_ground = "10.0.1.1";
#if ROLE
  strcpy(ifr.ifr_name,"airtun");
  addr.sin_addr.s_addr = inet_addr(addr_str_tunnel_board);
  dstaddr.sin_addr.s_addr = inet_addr(addr_str_tunnel_ground);
#else
  strcpy(ifr.ifr_name, "grdtun");
  addr.sin_addr.s_addr = inet_addr(addr_str_tunnel_ground);
  dstaddr.sin_addr.s_addr = inet_addr(addr_str_tunnel_board);
#endif // ROLE
  if (0 > (param->fd[dev]=open("/dev/net/tun",O_RDWR))) exit(-1);
  ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
  if (ioctl( param->fd[dev], TUNSETIFF, &ifr ) < 0 ) exit(-1);
  if (-1 == (fd_tun_udp=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP))) exit(-1);
  addr.sin_family = AF_INET;
  memcpy(&ifr.ifr_addr,&addr,sizeof(struct sockaddr));
  if (ioctl( fd_tun_udp, SIOCSIFADDR, &ifr ) < 0 ) exit(-1);
  addr.sin_addr.s_addr = inet_addr( "255.255.255.0");
  memcpy(&ifr.ifr_addr,&addr,sizeof(struct sockaddr));
  if (ioctl( fd_tun_udp, SIOCSIFNETMASK, &ifr ) < 0 ) exit(-1);
  dstaddr.sin_family = AF_INET;
  memcpy(&ifr.ifr_addr,&dstaddr,sizeof(struct sockaddr));
  if (ioctl( fd_tun_udp, SIOCSIFDSTADDR, &ifr ) < 0 ) exit(-1);
  ifr.ifr_mtu = TUN_MTU;
  if (ioctl( fd_tun_udp, SIOCSIFMTU, &ifr ) < 0 ) exit(-1);
  ifr.ifr_flags = IFF_UP ;
  if (ioctl( fd_tun_udp, SIOCSIFFLAGS, &ifr ) < 0 ) exit(-1);
  FD_SET(param->fd[dev], &(param->readset_all[0]));if (param->maxfd_all[0] < param->fd[dev]) param->maxfd_all[0]=param->fd[dev];
  FD_SET(param->fd[dev], &(param->readset_all[1]));if (param->maxfd_all[1] < param->fd[dev]) param->maxfd_all[1]=param->fd[dev];
  FD_SET(param->fd[dev], &(param->readset_all[2]));if (param->maxfd_all[2] < param->fd[dev]) param->maxfd_all[2]=param->fd[dev];


  dev=VID1_FD;   // Video (one unidirectional link)
  if (-1 == (param->fd[dev]=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP))) exit(-1);
#if ROLE
  addr_in[dev].sin_family = AF_INET;
  addr_in[dev].sin_port = htons(5600);
  addr_in[dev].sin_addr.s_addr = inet_addr(ADDR_LOCAL);
  if (-1 == bind(param->fd[dev], (struct sockaddr *)&addr_in[dev], sizeof(addr_in))) exit(-1);
  FD_SET(param->fd[dev], &(param->readset_all[0]));if (param->maxfd_all[0] < param->fd[dev]) param->maxfd_all[0]=param->fd[dev];
  FD_SET(param->fd[dev], &(param->readset_all[1]));if (param->maxfd_all[1] < param->fd[dev]) param->maxfd_all[1]=param->fd[dev];
  FD_SET(param->fd[dev], &(param->readset_all[2]));if (param->maxfd_all[2] < param->fd[dev]) param->maxfd_all[2]=param->fd[dev];
#else
  param->addr_out[dev].sin_family = AF_INET;
  param->addr_out[dev].sin_port = htons(5600);
  param->addr_out[dev].sin_addr.s_addr = inet_addr(ADDR_LOCAL);
#endif // ROLE
       
       
  return(ret);
}


#endif // WFB_H
