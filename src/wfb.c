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

#if BOARD
#include "fec.h"
#endif // BOARD

extern wfb_utils_pay_t wfb_utils_pay;

extern wfb_utils_down_t wfb_utils_down[2];

/*****************************************************************************/
int main(void)
{
  int8_t raw;
  uint8_t tmp, devout, readcpt=0, devcpt, readtab[FD_NB];
  uint8_t num=0, seq=0;
  uint64_t exptime;
  ssize_t len;

#define rawmsgnb 2 // { RAW0_FD, RAW1_FD }
  const uint8_t rawmsgstoresize = 1 + (2 * (1 + FEC_N));
  wfb_utils_rawmsg_t rawmsg[rawmsgnb][rawmsgstoresize];
  uint8_t rawcur=0;

#define msgnb 5    // { WFB_FD, TUN_FD, VID1_FD, VID2_FD, TEL_FD }
  uint8_t msgbuf[rawmsgnb][msgnb][FEC_N][ONLINE_MTU];
  struct iovec msg[rawmsgnb][msgnb][FEC_N], *ptrmsg;
  for (uint8_t i=0; i < rawmsgnb; i++) {
    for (uint8_t j=0; j < msgnb; j++) {
      for (uint8_t k=0; k < FEC_N; k++) {
        msg[i][j][k].iov_base = &msgbuf[i][j][k][0];
      }
    }
  }
  struct iovec *downmsg[2] = { &msg[0][0][0], &msg[1][0][0] };

  fec_init();
  fec_t *fec_p = fec_new(FEC_K, FEC_N);
  int16_t numprev[2]={-1,-1};
  uint8_t *pdebug;

#if BOARD
  uint8_t vidnum[2]={0,0};
  uint8_t vidseq[2]={1,1};
  unsigned blocknums[FEC_N-FEC_K]; for(uint8_t i=0; i<(FEC_N-FEC_K); i++) blocknums[i]=(i+FEC_K);
  uint8_t *datablocks[FEC_K], *fecblocks[FEC_N-FEC_K];
#else
  wfb_utils_dispatchvideo_t dspvid[2];
  for (uint8_t i=0; i < 2; i++) { dspvid[i].k_in = 0; dspvid[i].fec = 0; dspvid[i].seq = -1; dspvid[i].recfec = -1; 
	                          dspvid[i].recseq = 0;
	                          memset(dspvid[i].blk, 0, VIDBLKSIZE * sizeof(wfb_utils_rawmsg_t *)); }
#endif // BOARD
       
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
    downmsg[0]->iov_base = &wfb_utils_down[0];
    downmsg[1]->iov_base = &wfb_utils_down[1];
    downmsg[0]->iov_len = sizeof(wfb_utils_down_t);
    downmsg[1]->iov_len = sizeof(wfb_utils_down_t);
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
	      if (vidnum[devcpt - VID1_FD] < FEC_K) {
                ptrmsg = &msg[0][devcpt - WFB_FD][vidnum[devcpt - VID1_FD]];
		memset(ptrmsg->iov_base, 0, ONLINE_MTU);
                ptrmsg->iov_base = &msgbuf[0][devcpt - WFB_FD][vidnum[devcpt - VID1_FD]][sizeof(wfb_utils_fec_t)];
                ptrmsg->iov_len  = PAY_MTU;
                len = readv( dev[devcpt].fd, ptrmsg, 1);
                ptrmsg->iov_base = &msgbuf[0][devcpt - WFB_FD][vidnum[devcpt - VID1_FD]][0];
		ptrmsg->iov_len = len + sizeof(wfb_utils_fec_t);
                memcpy(&msgbuf[0][devcpt - WFB_FD][vidnum[devcpt - VID1_FD]][0], &len, sizeof(wfb_utils_fec_t));
		vidnum[devcpt - VID1_FD]++;
/*
                pdebug = (uint8_t *)&msgbuf[0][devcpt - WFB_FD][vidnum[devcpt - VID1_FD] - 1][sizeof(wfb_utils_fec_t)];
                printf("readv fec(%d)  %02X[%ld]%02X\n", vidnum[devcpt - VID1_FD] - 1,
                  *(14 + pdebug), 
		  len,
                  *(len + pdebug - 1));
*/
	      }
	      break;
#else
#endif // BOARD
     
#if TELEM
            case TEL_FD:
#endif // TELEM
	    case TUN_FD:
	      ptrmsg = &msg[0][devcpt - WFB_FD][0];
              ptrmsg->iov_len = ONLINE_MTU;
              ptrmsg->iov_len = readv( dev[devcpt].fd, ptrmsg, 1);
	      break;

            case RAW0_FD:
            case RAW1_FD:
              wfb_utils_presetrawmsg(&rawmsg[devcpt - RAW0_FD][rawcur], ONLINE_MTU, true);
              len = recvmsg( dev[devcpt].fd, &rawmsg[devcpt - RAW0_FD][rawcur].msg, MSG_DONTWAIT);
              if (!((len > 0)&&(wfb_utils_pay.droneid == DRONEID))) {
#if BOARD
	        wfbstat.stat[devcpt - RAW0_FD].incoming++;
#endif // BOARD
	      } else {

                if (numprev[devcpt - RAW0_FD] >= 0) {
		  if (numprev[devcpt - RAW0_FD] == 255) {
		    if (wfb_utils_pay.num != 0) {
  		      wfbstat.stat[devcpt - RAW0_FD].fails++;
		    }
		  } else if ((1 + (numprev[devcpt - RAW0_FD])) != wfb_utils_pay.num) {
  		    wfbstat.stat[devcpt - RAW0_FD].fails++;
  		  } 
  		}

		numprev[devcpt - RAW0_FD] = wfb_utils_pay.num;
		devout = WFB_FD + wfb_utils_pay.msgcpt;
		if (wfb_utils_pay.msglen > 0) {
		  switch(devout) {
#if BOARD
#else
		    case VID1_FD:
		    case VID2_FD:
                      wfb_utils_dispatchvideo(&dev[devout], &(wfbstat.stat[devcpt - RAW0_FD].dev[devout]), 
				              &rawmsg[devcpt - RAW0_FD][rawcur], &dspvid[devout - VID1_FD], fec_p);
                      if (rawcur == (rawmsgstoresize - 1)) rawcur=0;
                      else rawcur++;
		      break;

                    case WFB_FD:
                      wfbstat.stat[devcpt - RAW0_FD].incoming++;
		      wfbstat.stat[devcpt - RAW0_FD].dev[devout].rcv += wfb_utils_pay.msglen;
		      wfbstat.stat[devcpt - RAW0_FD].chan = 
		        ((wfb_utils_down_t *)rawmsg[devcpt - RAW0_FD][rawcur].headvecs.head[wfb_utils_datapos].iov_base)->chan;
	              break;
#endif // BOARD
#if TELEM
                    case TEL_FD:
#if BOARD
#else
		      if ((len = sendto(dev[devout].fd,
		        rawmsg[devcpt - RAW0_FD][rawcur].headvecs.head[wfb_utils_datapos].iov_base,
		        wfb_utils_pay.msglen,
		        MSG_DONTWAIT, (struct sockaddr *)&(dev[devout].addrout), sizeof(struct sockaddr))) > 0) {
		          wfbstat.stat[devcpt - RAW0_FD].dev[devout].rcv += len;
		      }
#endif // BOARD
#endif // TELEM
                    case TUN_FD:
                      if ((len = write(dev[devout].fd, 
		        rawmsg[devcpt - RAW0_FD][rawcur].headvecs.head[wfb_utils_datapos].iov_base,
		        wfb_utils_pay.msglen)) > 0) {
		          wfbstat.stat[devcpt - RAW0_FD].dev[devout].rcv += len;
		      }

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
        } // if readset POLLIN
      } // end for 

      for (uint8_t j=0; j < msgnb; j++) {
        if (bckup) tmp=2; else tmp=1;

        for (uint8_t i=0; i<tmp; i++) {
	  uint8_t k=0;
#if BOARD
	  if ((j == (VID1_FD - WFB_FD)) || (j == (VID2_FD - WFB_FD))) {
	    if (vidnum[ j - VID1_FD + WFB_FD] == FEC_K) {
              for (uint8_t f=0; f<FEC_K; f++) datablocks[f] = (uint8_t *)&msgbuf[i][j][f];
              for (uint8_t f=0; f<(FEC_N - FEC_K); f++) {
	        fecblocks[f] = (uint8_t *)&msgbuf[i][j][f + FEC_K];
                msg[i][j][f + FEC_K].iov_len = ONLINE_MTU;
	      }
	      fec_encode(fec_p,
			 (const gf*restrict const*restrict const)datablocks,
			 (gf*restrict const*restrict const)fecblocks,
			 (const unsigned*restrict const)blocknums, (FEC_N-FEC_K), ONLINE_MTU);
	    }
	  } 
	  if ((j == (VID1_FD - WFB_FD)) || (j == (VID2_FD - WFB_FD)))
	    k = (vidnum[ j - VID1_FD + WFB_FD ] - 1);
#endif // BOARD
          while (msg[i][j][k].iov_len > 0) {
	    raw = wfbstat.raw[i];
            if (raw < 0) msg[i][j][k].iov_len = 0; 
	    else {
#if BOARD
	      if ((j == (VID1_FD - WFB_FD)) || (j == (VID2_FD - WFB_FD)))
	        seq = vidseq[ j - VID1_FD + WFB_FD];
#endif // BOARD
              wfb_utils_presetrawmsg(&rawmsg[raw][0], ONLINE_MTU, false);
              wfb_utils_pay.msgcpt = j;
              wfb_utils_pay.droneid = DRONEID;
              wfb_utils_pay.seq = seq;
              wfb_utils_pay.fec = k;
              wfb_utils_pay.num = num++;
              wfb_utils_pay.msglen = msg[i][j][k].iov_len ;
  
      	      rawmsg[raw][0].headvecs.head[wfb_utils_datapos] = msg[i][j][k];
  #if RAW
  #else
      	      rawmsg[raw][0].msg.msg_name = &dev[raw + RAW0_FD].addrout;
      	      rawmsg[raw][0].msg.msg_namelen = sizeof(dev[raw + RAW0_FD].addrout);
  #endif // RAW
              len = sendmsg(dev[raw + RAW0_FD].fd, &rawmsg[raw][0].msg, MSG_DONTWAIT);
        	      wfbstat.stat[raw].dev[j + WFB_FD].snd += msg[i][j][k].iov_len ;
/*
              pdebug = (uint8_t *)rawmsg[raw][0].headvecs.head[wfb_utils_datapos].iov_base;
              printf("sendmsg num(%d) seq(%d) fec(%d) len(%ld) %02X[%ld]%02X\n",num-1,seq,k,len,
                  *(16 + pdebug),
                  rawmsg[raw][0].headvecs.head[wfb_utils_datapos].iov_len,
                  *(rawmsg[raw][0].headvecs.head[wfb_utils_datapos].iov_len + pdebug - 1));
*/
#if BOARD
	      if ((j == (VID1_FD - WFB_FD)) || (j == (VID2_FD - WFB_FD))) {
                if ((k == (FEC_N - 1)) && (msg[i][j][FEC_N - 1].iov_len > 0)) {
		  vidnum[ j - VID1_FD + WFB_FD] = 0; 
		  vidseq[ j - VID1_FD + WFB_FD]++; 
                }
	      }
#endif // BOARD
              msg[i][j][k].iov_len = 0;
	      k++;
	    }
	  }
        }
      }
//      if (seq==5) exit(-1); DEBUG
    } // end poll
  }
}
