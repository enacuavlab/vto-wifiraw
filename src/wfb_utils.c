#include <dirent.h>

#include <netlink/route/link.h>
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <linux/nl80211.h>

#include "wfb_utils.h"

uint16_t g_family;
uint32_t g_iftype;

uint8_t g_freqsnb;
uint32_t g_freqs[freqsmax];
uint32_t g_chans[freqsmax];

#if RAW
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
#endif // RAW


/*****************************************************************************/
bool wfb_utils_init(wfb_utils_t *param){
//  memset(param,0,sizeof(struct wfb_utils_t));
#if ROLE
  memset(&(param->wfbdown),0,sizeof(wfbdown_t));
#else
  param->wfbtosendup = false;
  memset(&(param->wfbup),0,sizeof(wfbup_t));
#endif // ROLE
  param->repeat = 0;
  param->seq_out = 0;
  param->unlockfreq = true;
  param->nofaultsec = 0;
  param->sync_active = false;
  param->datatosend = false;
  param->ping = 0;

#if RAW
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

  return(ret);
  nla_put_failure:
    nlmsg_free(msg);
    return (false);

#else // RAW  
  uint32_t static_freqchans[2][58] = {
    {2412, 2417, 2422, 2427, 2432, 2437, 2442, 2447, 2452, 2457, 2462, 2467, 2472, 2484, 5075, 5080, 5085, 5090, 5100, \
    5120, 5140, 5160, 5180, 5200, 5220, 5240, 5260, 5280, 5300, 5320, 5340, 5360, 5380, 5400, 5420, 5440, 5460, 5480, \
    5500, 5520, 5540, 5560, 5580, 5600, 5620, 5640, 5660, 5680, 5700, 5720, 5745, 5765, 5785, 5805, 5825, 5845, 5865, 5885},
    {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64, 68, \
    72, 76, 80, 84, 88, 92, 96, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 149, 153, 157, 161, 165, 169, 173, 177}
  };
  strcpy(param->name,"dummyname");
  param->freqsnb = 59;
  memcpy(param->freqs,static_freqchans[0],(param->freqsnb-1)*sizeof(uint32_t));
  memcpy(param->chans,static_freqchans[1],(param->freqsnb-1)*sizeof(uint32_t));

  return(true);  
#endif // RAW
}

/*****************************************************************************/
bool wfb_utils_setfreq(int freqcpt,wfb_utils_t *param) {
  bool ret=true;
  param->freqcptcur = freqcpt;
#if RAW
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
#else // RAW
  return(ret);  
#endif // RAW
}
