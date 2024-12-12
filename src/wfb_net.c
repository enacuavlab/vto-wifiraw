#include <netlink/route/link.h>
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>

#include <linux/nl80211.h>
#include <linux/if_packet.h>
#include <linux/filter.h>

#include <net/ethernet.h>
#include <net/if.h>

#include <sys/ioctl.h>
#include <sys/types.h>


#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>

#include "wfb_net.h"


uint8_t g_freqsnb;
uint16_t g_family;
uint32_t g_freqs[freqsmax];
uint32_t g_chans[freqsmax];

uint32_t g_iftype;


struct iovec wfb_net_ieeehd_vec = { .iov_base = &wfb_net_ieeehd, .iov_len = sizeof(wfb_net_ieeehd)};
struct iovec wfb_net_radiotaphd_tx_vec = { .iov_base = &wfb_net_radiotaphd_tx, .iov_len = sizeof(wfb_net_radiotaphd_tx)};
struct iovec wfb_net_radiotaphd_rx_vec = { .iov_base = &wfb_net_radiotaphd_rx, .iov_len = sizeof(wfb_net_radiotaphd_rx)};

/*****************************************************************************/
static int handler_get_index_type(struct nl_msg *msg, void *arg) {
  struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];
  struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));

  nla_parse(tb_msg,NL80211_ATTR_MAX,genlmsg_attrdata(gnlh,0),genlmsg_attrlen(gnlh,0),NULL);
  if (tb_msg[NL80211_ATTR_IFNAME]) printf("Interface %s\n", nla_get_string(tb_msg[NL80211_ATTR_IFNAME]));
  if (tb_msg[NL80211_ATTR_IFINDEX]) printf("ifindex %d\n", nla_get_u32(tb_msg[NL80211_ATTR_IFINDEX]));
  if (tb_msg[NL80211_ATTR_IFTYPE]) {
    g_iftype = nla_get_u32(tb_msg[NL80211_ATTR_IFTYPE]);
  }
  return NL_SKIP;
}

/*****************************************************************************/
static int handler_get_channels(struct nl_msg *msg, void *arg) {
  struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
  struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];
  struct nlattr *tb_band[NL80211_BAND_ATTR_MAX + 1];
  struct nlattr *tb_freq[NL80211_FREQUENCY_ATTR_MAX + 1];
  struct nlattr *nl_band;
  struct nlattr *nl_freq;
  int rem_band, rem_freq;
  static int last_band = -1;
  uint8_t cpt=0;

  nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0), genlmsg_attrlen(gnlh, 0), NULL);
  if (tb_msg[NL80211_ATTR_WIPHY_BANDS]) {
    g_freqsnb = 0;
    nla_for_each_nested(nl_band, tb_msg[NL80211_ATTR_WIPHY_BANDS], rem_band) {
      if (last_band != nl_band->nla_type) {
        last_band = nl_band->nla_type;
      }
      nla_parse(tb_band, NL80211_BAND_ATTR_MAX, nla_data(nl_band), nla_len(nl_band), NULL);
      if (tb_band[NL80211_BAND_ATTR_FREQS]) {
        nla_for_each_nested(nl_freq, tb_band[NL80211_BAND_ATTR_FREQS], rem_freq) {
          nla_parse(tb_freq, NL80211_FREQUENCY_ATTR_MAX, nla_data(nl_freq), nla_len(nl_freq), NULL);
          g_freqs[g_freqsnb] = nla_get_u32(tb_freq[NL80211_FREQUENCY_ATTR_FREQ]);
          if (g_freqs[g_freqsnb] == 2484) g_chans[cpt] = 14;
          else if (g_freqs[g_freqsnb] < 2484) g_chans[g_freqsnb] = (g_freqs[g_freqsnb] - 2407) / 5;
          else if (g_freqs[g_freqsnb] < 5000) g_chans[g_freqsnb] = 15 + ((g_freqs[g_freqsnb] - 2512) / 20);
          else g_chans[g_freqsnb] = ((g_freqs[g_freqsnb] - 5000) / 5);
          g_freqsnb++;
        }
      }
    }
  }
  return NL_SKIP;
}

/*****************************************************************************/
bool wfb_net_init(wfb_net_init_t *param) {

  bool ret=false;
  DIR *d1,*d2;
  FILE *fd;
  char path[1024],buf[1024];
  ssize_t lenlink;
  struct dirent *dir1,*dir2;
  char *ptr,*netpath = "/sys/class/net";

  struct nl_sock *sk_rt;
  struct rtnl_link *link, *change;
  struct nl_cache *cache;
  int err = 0;

  struct nl_msg *msg = nlmsg_alloc();
  if ((sk_rt = nl_socket_alloc()) && (param->sk_nl = nl_socket_alloc())) {
    if (((err = nl_connect(sk_rt, NETLINK_ROUTE)) >= 0) && ((err = genl_connect(param->sk_nl)) >= 0)) {
      g_family = genl_ctrl_resolve(param->sk_nl, "nl80211");
      if ((err = rtnl_link_alloc_cache(sk_rt, AF_UNSPEC, &cache)) >= 0) {
        d1 = opendir(netpath);
        while ((dir1 = readdir(d1)) != NULL) {
          sprintf(path,"%s/%s/device/driver",netpath,dir1->d_name);
          if ((lenlink = readlink(path, buf, sizeof(buf)-1)) != -1) {
            buf[lenlink] = '\0';
            ptr = strrchr( buf, '/' );
            if ((strncmp("rtl88XXau",(ptr+1),9)) == 0) {
              sprintf(path,"%s/%s/phy80211",netpath,dir1->d_name);
              if ((lenlink = readlink(path, buf, sizeof(buf)-1)) != -1) {
                buf[lenlink] = '\0';
                ptr = strrchr( buf, '/' );
                sprintf(path,"%s/%s/phy80211",netpath,dir1->d_name);
                d2 = opendir(path);
                while ((dir2 = readdir(d2)) != NULL)
                  if ((strncmp("rfkill",dir2->d_name,6)) == 0) break;
                if ((strncmp("rfkill",dir2->d_name,6)) == 0) {
                  sprintf(path,"%s/%s/phy80211/%s/soft",netpath,dir1->d_name,dir2->d_name);
                  fd = fopen(path,"r+");
                  if (fgetc(fd)==49) {
                    fseek(fd, -1, SEEK_CUR);
                    fputc(48, fd);
                  };
                  fclose(fd);
                }
                if ((link = rtnl_link_get_by_name(cache, dir1->d_name))) {
                  if (!(rtnl_link_get_flags (link) & IFF_UP)) {
                    change = rtnl_link_alloc ();
                    rtnl_link_set_flags (change, IFF_UP);
                    rtnl_link_change(sk_rt, link, change, 0);

                    param->ifind = if_nametoindex(dir1->d_name);
                    nl_socket_modify_cb(param->sk_nl,NL_CB_VALID,NL_CB_CUSTOM,handler_get_index_type,NULL);
                    g_iftype = 0;
                    genlmsg_put(msg,0,0,g_family,0,NLM_F_DUMP,NL80211_CMD_GET_INTERFACE,0);
                    NLA_PUT_U32(msg,NL80211_ATTR_IFINDEX,if_nametoindex(dir1->d_name));
                    if (nl_send_auto(param->sk_nl, msg) >= 0) {
                      nl_recvmsgs_default(param->sk_nl);
                      if (g_iftype != NL80211_IFTYPE_MONITOR) {
                        nlmsg_free(msg);
                        msg = nlmsg_alloc();
                        genlmsg_put(msg,0,0,g_family,0,0,NL80211_CMD_SET_INTERFACE,0);
                        NLA_PUT_U32(msg,NL80211_ATTR_IFINDEX,if_nametoindex(dir1->d_name));
                        NLA_PUT_U32(msg,NL80211_ATTR_IFTYPE,NL80211_IFTYPE_MONITOR);
                        if (nl_send_auto(param->sk_nl, msg) >= 0) {
                          nl_recvmsgs_default(param->sk_nl);
                          nl_socket_modify_cb(param->sk_nl,NL_CB_VALID,NL_CB_CUSTOM,handler_get_channels,NULL);
                          nlmsg_free(msg);
                          msg = nlmsg_alloc();
                          genlmsg_put(msg,0,0,g_family,0,0,NL80211_CMD_GET_WIPHY,0);
                          NLA_PUT_U32(msg,NL80211_ATTR_IFINDEX,if_nametoindex(dir1->d_name));
                          if (nl_send_auto(param->sk_nl, msg) >= 0) {
                            nl_recvmsgs_default(param->sk_nl);
                            strcpy(param->name,dir1->d_name);
                            param->freqsnb = g_freqsnb;
                            memcpy(param->freqs,g_freqs,(param->freqsnb-1)*sizeof(uint32_t));
                            memcpy(param->chans,g_chans,(param->freqsnb-1)*sizeof(uint32_t));
                            ret=true;
                            break;
                          }
                        }
                      }
                    }
                  }
                }
                closedir(d2);
              }
            }
          }
        }
        closedir(d1);
      }
    }
  }
  if (ret) {
    //uint8_t flags;
    uint16_t protocol = htons(ETH_P_ALL);
    if (-1 == (param->fd = socket(AF_PACKET,SOCK_RAW,protocol))) exit(-1);
//    if (-1 == (flags = fcntl(param->fd, F_GETFL))) exit(-1);
//    if (-1 == (fcntl(param->fd, F_SETFL, flags | O_NONBLOCK))) exit(-1);
    struct sock_filter zero_bytecode = BPF_STMT(BPF_RET | BPF_K, 0);
    struct sock_fprog zero_program = { 1, &zero_bytecode};
    if (-1 == setsockopt(param->fd, SOL_SOCKET, SO_ATTACH_FILTER, &zero_program, sizeof(zero_program))) exit(-1);
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy( ifr.ifr_name, param->name, sizeof( ifr.ifr_name ) - 1 );
    if (ioctl( param->fd, SIOCGIFINDEX, &ifr ) < 0 ) exit(-1);
    struct sockaddr_ll sll;
    memset( &sll, 0, sizeof( sll ) );
    sll.sll_family   = AF_PACKET;
    sll.sll_ifindex  = ifr.ifr_ifindex;
    sll.sll_protocol = protocol;
    if (-1 == bind(param->fd, (struct sockaddr *)&sll, sizeof(sll))) exit(-1);
    char drain[1];
    while (recv(param->fd, drain, sizeof(drain), MSG_DONTWAIT) >= 0) {
      printf("----\n");
    };
    struct sock_filter full_bytecode = BPF_STMT(BPF_RET | BPF_K, (u_int)-1);
    struct sock_fprog full_program = { 1, &full_bytecode};
    if (-1 == setsockopt(param->fd, SOL_SOCKET, SO_ATTACH_FILTER, &full_program, sizeof(full_program))) ret=false;
    static const int32_t sock_qdisc_bypass = 1;
    if (-1 == setsockopt(param->fd, SOL_PACKET, PACKET_QDISC_BYPASS, &sock_qdisc_bypass, sizeof(sock_qdisc_bypass))) ret=false;
  }

  return(ret);
  nla_put_failure:
    nlmsg_free(msg);
    return (false);
}

/*****************************************************************************/
bool wfb_net_setfreq(uint8_t freqcpt,wfb_net_init_t *param) {
  bool ret=true;
  param->freqcptcur = freqcpt;
  struct nl_msg *msg=nlmsg_alloc();
  genlmsg_put(msg,0,0,g_family,0,0,NL80211_CMD_SET_CHANNEL,0);
  NLA_PUT_U32(msg,NL80211_ATTR_IFINDEX,param->ifind);
  NLA_PUT_U32(msg,NL80211_ATTR_WIPHY_FREQ,param->freqs[freqcpt]);
  if (nl_send_auto(param->sk_nl, msg) < 0) ret=false;
  nlmsg_free(msg);
  return(ret);
  nla_put_failure:
    nlmsg_free(msg);
    return(false);
}

/*****************************************************************************/
void wfb_net_incfreq(uint8_t avoidfreqcpt, wfb_net_init_t *param) {
  if(param->freqcptcur < (param->freqsnb - 1)) ++(param->freqcptcur);
  else param->freqcptcur = 0;
  if ( param->freqcptcur == avoidfreqcpt)  ++(param->freqcptcur); 
  wfb_net_setfreq( param->freqcptcur, param);
}
