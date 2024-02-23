#ifndef WFB_UTILS_H
#define WFB_UTILS_H

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include <net/if.h>

#define freqsmax 65

typedef struct {
  int8_t chan;
  int8_t backupchan;
  bool mainchan;
  int8_t antdbm;
  uint16_t temp;
  uint32_t fails;
//  uint32_t drops;
  uint32_t sent;
//  uint32_t rate;
} __attribute__((packed)) wfbdown_t;

typedef struct {
  int8_t backupchan;
} __attribute__((packed)) wfbup_t;

typedef struct wfb_utils_t {
  char name[20];
  uint8_t repeat;
  uint8_t freqcptcur;
  uint8_t freqsnb;
  uint16_t seq_out;
  uint32_t freqs[freqsmax];
  uint32_t chans[freqsmax];
  bool sync_active;
  bool datatosend;
  wfbdown_t wfbdown;
  bool wfbtosendup;
  wfbup_t wfbup;
#ifdef RAW
  int8_t antdbm;
  uint16_t ifind;
  uint32_t fails;
  bool unlockfreq;
  uint8_t nofaultsec;
  struct nl_sock *sk_nl;
#endif // RAW
} wfb_utils_t;
bool wfb_utils_init(wfb_utils_t *param);
bool wfb_utils_setfreq(int freqcpt, wfb_utils_t *param);

#endif // WFB_UTILS_H 
