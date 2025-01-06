#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/uio.h>

#include "wfb.h"
#include "wfb_utils.h"

extern wfb_utils_pay_t wfb_utils_pay;

extern wfb_utils_down_t wfb_utils_down[2];

/*****************************************************************************/
int main(void)
{
  int8_t raw;
  uint8_t tmp, devout, readcpt=0, devcpt, readtab[FD_NB];
  uint32_t num=0, numprev=0;
  uint64_t exptime;
  ssize_t len;

#define rawmsgnb 2
  wfb_utils_rawmsg_t rawmsg[rawmsgnb]; // { RAW0_FD, RAW1_FD };
#define msgnb 5
  uint8_t msgbuf[rawmsgnb][msgnb][STORE_SIZE][ONLINE_MTU];
  wfb_utils_msg_t msg[rawmsgnb][msgnb], *ptrmsg; //{ WFB_FD, TUN_FD, VID1_FD, VID2_FD, TEL_FD };
  wfb_utils_presetmsg(rawmsgnb, msgnb, msgbuf, msg, STORE_SIZE);
  wfb_utils_msg_t *downmsg[2] = { &msg[0][0], &msg[1][0] };

  wfb_utils_stat_t wfbstat;
  wfb_utils_init_t dev[FD_NB];
  struct pollfd readsets[FD_NB];
  bool bckup = false;
  wfb_utils_init(dev, &readcpt, readtab, readsets, &bckup);
  if (!bckup) wfbstat.raw[0] = 0;
  else {
    wfbstat.raw[0] = 0; wfbstat.raw[1] = 1;
#if RAW
#if BOARD
    downmsg[0]->vecs[0].iov_base = &wfb_utils_down[0];
    downmsg[1]->vecs[0].iov_base = &wfb_utils_down[1];
    downmsg[0]->len = sizeof(wfb_utils_down_t);
    downmsg[1]->len = sizeof(wfb_utils_down_t);
    wfb_utils_down[0].chan = 100 + dev[wfbstat.raw[1]+RAW0_FD].raw.freqcptcur;
    wfb_utils_down[1].chan = dev[wfbstat.raw[0]+RAW0_FD].raw.freqcptcur;
#endif // BOARD
#endif // RAW
  }

  for(;;) {	
    if (0 != poll(readsets, readcpt, -1)) {
      for (uint8_t cpt=0; cpt<readcpt; cpt++) {
        if (readsets[cpt].revents == POLLIN) {
          devcpt = readtab[ cpt ];
          switch(devcpt) {
            case TIME_FD :  
              len = read( dev[devcpt].fd, &exptime, sizeof(uint64_t));
              wfb_utils_periodic(dev, bckup, downmsg, &wfbstat);
      	      break; 
#if BOARD
            case VID1_FD:
            case VID2_FD:
#else
#endif // BOARD
     
#if TELEM
            case TEL_FD:
#endif // TELEM
	    case TUN_FD:
	      ptrmsg = &msg[0][devcpt - WFB_FD];
              ptrmsg->len = readv( dev[devcpt].fd, &ptrmsg->vecs[0], STORE_SIZE);
	      break;

            case RAW0_FD:
            case RAW1_FD:
              wfb_utils_presetrawmsg(&rawmsg[devcpt - RAW0_FD], ONLINE_MTU, true);
              len = recvmsg( dev[devcpt].fd, &rawmsg[devcpt - RAW0_FD].msg, MSG_DONTWAIT);
              if (!((len > 0)&&(wfb_utils_pay.droneid == DRONEID))) {
#if BOARD
	        wfbstat.stat[devcpt - RAW0_FD].incoming++;
#endif // BOARD
	      } else {
                if (numprev != 0) if (++numprev != wfb_utils_pay.num) wfbstat.stat[devcpt - RAW0_FD].fails++;
		numprev = wfb_utils_pay.num;
		devout = WFB_FD + wfb_utils_pay.msgcpt;
		if (wfb_utils_pay.msglen > 0) {
		  switch(devout) {
#if TELEM
                    case TEL_FD:
#endif // TELEM
#if BOARD
#else
		    case VID1_FD:
		    case VID2_FD:
                      if ((len = sendto(dev[devout].fd, 
			      rawmsg[devcpt - RAW0_FD].headvecs.head[wfb_utils_datapos].iov_base, wfb_utils_pay.msglen,
			      MSG_DONTWAIT, (struct sockaddr *)&(dev[devout].addrout), sizeof(struct sockaddr))) > 0) 
		        wfbstat.stat[devcpt - RAW0_FD].dev[devout].rcv += len;
		      break;

                    case WFB_FD:
                      wfbstat.stat[devcpt - RAW0_FD].incoming++;
		      wfbstat.stat[devcpt - RAW0_FD].dev[devout].rcv += wfb_utils_pay.msglen;
		      wfbstat.stat[devcpt - RAW0_FD].chan = 
		        ((wfb_utils_down_t *)rawmsg[devcpt - RAW0_FD].headvecs.head[wfb_utils_datapos].iov_base)->chan;
	              break;
#endif // BOARD
                    case TUN_FD:
                      if ((len = write(dev[devout].fd, 
			      rawmsg[devcpt - RAW0_FD].headvecs.head[wfb_utils_datapos].iov_base, wfb_utils_pay.msglen)) > 0) 
		        wfbstat.stat[devcpt - RAW0_FD].dev[devout].rcv += len;
                      break;

                    default:
                      break;
		  }
	        }
	      }
	      break;

	    default:  
              break;
          }
        }
      } // end poll

      for (uint8_t j=0; j < msgnb; j++) {
        if (bckup) tmp=2; else tmp=1;
        for (uint8_t i=0; i<tmp; i++) {
          uint8_t k=0;
          while ((msg[i][j].len > 0) && (k < STORE_SIZE)) {
	    raw = wfbstat.raw[i];

            if (raw < 0) msg[i][j].len = 0; 
	    else {
              if (k==0) { 
                wfb_utils_presetrawmsg(&rawmsg[raw], ONLINE_MTU, false);
                wfb_utils_pay.msgcpt = j;
                wfb_utils_pay.droneid = DRONEID;
	      }
              wfb_utils_pay.num = ++num;
              if (msg[i][j].len < ONLINE_MTU) msg[i][j].vecs[k].iov_len = msg[i][j].len; 
              wfb_utils_pay.msglen = msg[i][j].len; 
    	      rawmsg[raw].headvecs.head[wfb_utils_datapos] = msg[i][j].vecs[k];
#if RAW
#else
    	      rawmsg[raw].msg.msg_name = &dev[raw + RAW0_FD].addrout;
    	      rawmsg[raw].msg.msg_namelen = sizeof(dev[raw + RAW0_FD].addrout);
#endif // RAW
              len = sendmsg(dev[raw + RAW0_FD].fd, &rawmsg[raw].msg, MSG_DONTWAIT);
      	      wfbstat.stat[raw].dev[j + WFB_FD].snd += msg[i][j].len ;
              msg[i][j].len -= len;
              msg[i][j].vecs[k].iov_len = ONLINE_MTU;
	      k++;
	    }
	  }
        }
      }
    }
  }
}
