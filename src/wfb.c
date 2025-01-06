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
#include "fec.h"

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
  wfb_utils_rawmsg_t rawmsg[rawmsgnb][FEC_N];
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
  uint8_t numprev[2]={0,0}, vidnum[2]={0,0,},vidseq[2]={0,0}, *pdebug, bufdebug[4][ONLINE_MTU]; memset(bufdebug, 0, sizeof(bufdebug));

#if BOARD
  unsigned blocknums[FEC_N-FEC_K]; for(uint8_t i=0; i<(FEC_N-FEC_K); i++) blocknums[i]=(i+FEC_K);
  uint8_t *datablocks[FEC_K], *fecblocks[FEC_N-FEC_K];
#else
  unsigned index[FEC_K];
  uint8_t outblocksbuf[FEC_N-FEC_K][ONLINE_MTU], *outblocks[FEC_N-FEC_K], *inblocks[FEC_K], vidblkin[rawmsgnb]={0,0};
  uint8_t *sdbuf, k_out = 0;
  int8_t k_in[2]={-1,-1};
  uint16_t sdlen;
  int8_t nextfec = -1;
  bool display=false, reset=false;
  const uint8_t vidblksize = (1 + FEC_N);
  wfb_utils_rawmsg_t *vidblk[rawmsgnb][ vidblksize ], *prawmsg;
#endif // BOARD
       
  wfb_utils_stat_t wfbstat;
  wfb_utils_init_t dev[FD_NB];
  struct pollfd readsets[FD_NB];
  bool bckup = false, failflag = false;
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
                if (numprev[devcpt - RAW0_FD] != 0) {
                  numprev[devcpt - RAW0_FD] = 1 + numprev[devcpt - RAW0_FD];
		  if (numprev[devcpt - RAW0_FD] != wfb_utils_pay.num) {
		    wfbstat.stat[devcpt - RAW0_FD].fails++;
		    failflag = true;
		  }
		}
		numprev[devcpt - RAW0_FD] = wfb_utils_pay.num;
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
                      rawmsg[devcpt - RAW0_FD][rawcur].headvecs.head[wfb_utils_datapos].iov_len = wfb_utils_pay.msglen;
                      if (vidseq[devout - VID1_FD] == 0) vidseq[devout - VID1_FD] = wfb_utils_pay.seq;
		      if (vidseq[devout - VID1_FD] == wfb_utils_pay.seq) {
		        vidblk[devout - VID1_FD][wfb_utils_pay.fec] = &rawmsg[devcpt - RAW0_FD][ rawcur ];
			if ((!vidblk[devout - VID1_FD][0])&&(nextfec<0)) {
			  if (rawcur > 0) nextfec = (rawcur-1); else nextfec = (vidblksize-1);
			}
		      } else {

			if ((!vidblk[devout - VID1_FD][0])&&(nextfec>=0)) vidblk[devout - VID1_FD][0]=&rawmsg[devcpt - RAW0_FD][nextfec];

                        uint8_t idx=0, j=FEC_K;
                        for (uint8_t k=0;k<FEC_K;k++) {
                          if (vidblk[devout - VID1_FD][k]) {
                            inblocks[k] = (uint8_t *)vidblk[devout - VID1_FD][k]->headvecs.head[wfb_utils_datapos].iov_base;
                            index[k] = k;
  			  } else {
                            while (( j < FEC_N ) && !(vidblk[devout - VID1_FD][j])) j++;
                            inblocks[k] = (uint8_t *)vidblk[devout - VID1_FD][j]->headvecs.head[wfb_utils_datapos].iov_base;
                            outblocks[idx] = &outblocksbuf[idx][0]; idx++;
                            index[k] = j;
  			    j++;
  			  }
  			}

    	                fec_decode(fec_p,
    			  (const gf*restrict const*restrict const)inblocks,
    			  (gf*restrict const*restrict const)outblocks,
    			  (const unsigned*restrict const)index, 
    			  ONLINE_MTU);

			display = true;
			k_out = FEC_K;
			if (k_in[devout - VID1_FD] == -1) k_in[devout - VID1_FD] = 0;
			else k_in[devout - VID1_FD] = k_in[devout - VID1_FD] + 1;
		        reset = true;
		      }

		      if ((wfb_utils_pay.fec < FEC_K) && (! failflag)) {
                        k_in[devout - VID1_FD] = wfb_utils_pay.fec; k_out = (wfb_utils_pay.fec + 1);
			failflag = false; display = true;
		      }

		      if (display) {
                        display = false;
  			for (uint8_t k = k_in[devout - VID1_FD]; k < k_out; k++) {
     			  prawmsg = vidblk[devout - VID1_FD][k];
                          if (prawmsg) {
                            sdbuf = (uint8_t *)(prawmsg->headvecs.head[wfb_utils_datapos].iov_base) + sizeof(wfb_utils_fec_t); 
                            sdlen = (prawmsg->headvecs.head[wfb_utils_datapos].iov_len) - sizeof(wfb_utils_fec_t); 
			  } else {
			    sdlen = ((wfb_utils_fec_t *)&outblocksbuf[k][0])->feclen;
                            sdbuf = &outblocksbuf[k][sizeof(wfb_utils_fec_t)];
			  }
                          if ((len = sendto(dev[devout].fd, sdbuf, sdlen, MSG_DONTWAIT, 
					      (struct sockaddr *)&(dev[devout].addrout), sizeof(struct sockaddr))) > 0) {
                            wfbstat.stat[devcpt - RAW0_FD].dev[devout].rcv += len;
                          }
			}
		      }

		      if (reset) {
                        reset = false;
			nextfec = -1;
			memset(&vidblk[devout - VID1_FD][0],0,vidblksize);
			vidseq[devout - VID1_FD] = wfb_utils_pay.seq;
		      }

		      if (rawcur == (vidblksize-1)) rawcur=0;
		      else rawcur++;
		      break;


                    case WFB_FD:
                      wfbstat.stat[devcpt - RAW0_FD].incoming++;
		      wfbstat.stat[devcpt - RAW0_FD].dev[devout].rcv += wfb_utils_pay.msglen;
		      wfbstat.stat[devcpt - RAW0_FD].chan = 
		        ((wfb_utils_down_t *)rawmsg[devcpt - RAW0_FD][rawcur].headvecs.head[wfb_utils_datapos].iov_base)->chan;
	              break;
#endif // BOARD
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
        }
      } 

      for (uint8_t j=0; j < msgnb; j++) {
        if (bckup) tmp=2; else tmp=1;
        for (uint8_t i=0; i<tmp; i++) {
#if BOARD
	  if ((j == (VID1_FD - WFB_FD)) || (j == (VID2_FD - WFB_FD))) {
	    if (vidnum[ j - VID1_FD + WFB_FD] == FEC_K) {
              for (uint8_t f=0; f<FEC_K; f++) datablocks[f] = (uint8_t *)&msgbuf[i][j][f];
              for (uint8_t f=0; f<(FEC_N - FEC_K); f++) {
	        fecblocks[f] = (uint8_t *)&msgbuf[i][j][f + FEC_K];
                msg[i][j][f + FEC_K].iov_len = ONLINE_MTU;
	      }
	      vidseq[ j - VID1_FD + WFB_FD]++;
	      seq = vidseq[ j - VID1_FD + WFB_FD];

	      fec_encode(fec_p,
			 (const gf*restrict const*restrict const)datablocks,
			 (gf*restrict const*restrict const)fecblocks,
			 (const unsigned*restrict const)blocknums, (FEC_N-FEC_K), ONLINE_MTU);
	    }
	  } 
#endif // BOARD
	  uint8_t k=0;
          while (msg[i][j][k].iov_len > 0) {
	    raw = wfbstat.raw[i];
            if (raw < 0) msg[i][j][k].iov_len = 0; 
	    else {
	      if ((j == (VID1_FD - WFB_FD)) || (j == (VID2_FD - WFB_FD))) {
	        if (vidnum[ j - VID1_FD + WFB_FD] < FEC_K) break;
		else if (k == (FEC_N -1)) vidnum[ j - VID1_FD + WFB_FD] = 0; 
	      }
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


              pdebug = (uint8_t *)rawmsg[raw][0].headvecs.head[wfb_utils_datapos].iov_base;
              printf("(%ld) sendmsg seq(%d) fec(%d) %02X[%ld]%02X\n",len,seq,k,
                *(16 + pdebug),
                rawmsg[raw][0].headvecs.head[wfb_utils_datapos].iov_len,
                *(rawmsg[raw][0].headvecs.head[wfb_utils_datapos].iov_len + pdebug - 1));


              msg[i][j][k].iov_len = 0;
	      k++;
	    }
	  }
        }
      }
    }
  }
}
