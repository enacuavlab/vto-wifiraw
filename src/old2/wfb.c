#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <sys/timerfd.h>

#include "wfb.h"
#include "wfb_utils.h"

#if FEC
#include "fec.h"
#endif

int main(void) {

#define FREESCANPERIOD 20
#if BOARD
#define FREETRANPERIOD 5
#define FREENBPKT 2
#define PERIOD_DELAY_S 1
#else
#define PERIOD_DELAY_S 2
#endif // BOARD

  uint64_t exptime;
  struct itimerspec period = { { PERIOD_DELAY_S, 0 }, { PERIOD_DELAY_S, 0 } };
  int16_t timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
  uint8_t cpt, cptfdstart = 0, nfds_timer_raw = 0, fd_arr[FD_NB];
  int8_t cptmain, cptbkup = -1, cpttmp;

  wfb_utils_t dev[ FD_NB ];
  if (wfb_utils_init( 1, &dev[ 1 ])) { // RAW1_FD
    if (INITCHAN1 < dev[ 1 ].freqsnb) {
      if (wfb_utils_setfreq( INITCHAN1 , &dev[ 1 ] )) {
	printf("ONE [%s]\n", dev[ 1 ].name);
	fd_arr[ 0 ] = TIME_FD;  fd_arr[ 1 ] = RAW1_FD;
	cptmain = 1; nfds_timer_raw = 1; cptfdstart = 1;
        if (wfb_utils_init( 0, &dev[ 0 ] )) { // RAW0_FD
	  if (INITCHAN0 < dev[ 0 ].freqsnb) {
            if (wfb_utils_setfreq( INITCHAN0, &dev[ 0 ] )) {
	      printf("TWO [%s]\n", dev[ 0 ].name);
	      fd_arr[ 0 ] = TIME_FD;  fd_arr[ 1 ] = RAW0_FD; fd_arr[ 2 ] = RAW1_FD;
	      cptmain = -1; nfds_timer_raw = 2; cptfdstart = 0;
	    } else exit(-1);
	  } else exit(-1);
	} 
      } else exit(-1);  
    } else exit(-1); 
  } else exit(-1); 

  uint8_t nfds_all = nfds_timer_raw, readlevel = nfds_timer_raw;
  wfb_addusr( dev, &nfds_all, fd_arr);

  struct pollfd readset_arr[3][FD_NB] = {
   { { .fd = timerfd, .events = POLLIN }, { .fd = dev[ 0 ].fd, .events = POLLIN } }, // RAW0
   { { .fd = timerfd, .events = POLLIN }, { .fd = dev[ 1 ].fd, .events = POLLIN } }, // RAW1
   { { .fd = timerfd, .events = POLLIN }, { .fd = dev[ 0 ].fd, .events = POLLIN }, { .fd = dev[ 1 ].fd, .events = POLLIN } },
  };
  for (uint8_t nb = nfds_timer_raw; nb < nfds_all; nb ++) {
    readset_arr[ 0 ][ nb + cptfdstart ].fd  = dev[ fd_arr[ nb + 1] ].fd; readset_arr[ 0 ][ nb + cptfdstart ].events = POLLIN ;
    readset_arr[ 1 ][ nb + cptfdstart ].fd  = dev[ fd_arr[ nb + 1] ].fd; readset_arr[ 1 ][ nb + cptfdstart ].events = POLLIN ;
  }
  if (nfds_timer_raw == 2) {
    for (uint8_t nb = (1 + nfds_timer_raw); nb <= nfds_all; nb ++) {
      readset_arr[ 2 ][ nb ].fd  = dev[ fd_arr[ nb ] ].fd; readset_arr[ 2 ][ nb ].events = POLLIN ;
    }
  }

  bool expired = false, crcok = false;
  int16_t retpoll, retwrite;
  uint8_t onlinebuff[ 2 ][ FD_NB ][ ONLINE_SIZE ], *ptr, id, lenlog=0, wfbmsg[300], cptonline;
  uint16_t offset, dst, src ;
  uint16_t nbbytes[FD_NB] = {0}, nbbytes_crcko = 0, nbbytes_crcok = 0;

  ssize_t lensum, len = 0, lentab[ 2 ][ FD_NB ];
  memset(&lentab,0, 2 * FD_NB * sizeof(ssize_t));

  uint16_t offsetraw = sizeof(ieeehdr);

#if FEC
uint8_t fecbuff[ 2 ][ 256 ][ ONLINE_SIZE ];
fec_t* fec_p = fec_new(4, 8);
#endif // FEC
       
#if RAW
  offsetraw += sizeof(radiotaphdr);
  extern uint32_t crc32_table[256];
  uint8_t nbfds;
  uint16_t radiotapvar, offset_droneid=0, datalen;
  uint32_t crc, crc_rx;
  extern uint32_t crc32_table[256];
#endif // RAW 

  timerfd_settime(timerfd, 0, &period, NULL);
  for(;;) {
    if (cptmain == -1) nbfds = 1 + nfds_timer_raw;
    else nbfds = 1 + nfds_all;

    if (0 != poll((struct pollfd *)&readset_arr[ readlevel ], nbfds, -1)) {
      for (uint8_t cptpoll = 0; cptpoll < nbfds; cptpoll++) { 
        if ((readset_arr[ readlevel ][ cptpoll ].revents == POLLIN)) {
          cpt = fd_arr[ cptpoll ];
          switch (cpt) {

            case TIME_FD :					      
              len = read( timerfd, &exptime, sizeof(uint64_t));
#if BOARD
#else
              nbbytes_crcok/=2; nbbytes_crcko/=2; nbbytes[WFB_FD]/=2; nbbytes[TUN_FD]/=2; nbbytes[VID1_FD]/=2; nbbytes[VID2_FD]/=2; nbbytes[TEL_FD]/=2;
#endif // BOARD
	      lenlog += sprintf((char *)wfbmsg + lenlog, "Bytrates OK(%d) KO(%d) WFB(%d) TUN(%d) VID1(%d) VID2(%d) TEL(%d)\n", \
                                nbbytes_crcok, nbbytes_crcko, nbbytes[WFB_FD], nbbytes[TUN_FD], nbbytes[VID1_FD], nbbytes[VID2_FD], nbbytes[TEL_FD]);
              nbbytes_crcok=0;nbbytes_crcko=0;nbbytes[WFB_FD]=0;nbbytes[TUN_FD]=0;nbbytes[VID1_FD]=0;nbbytes[VID2_FD]=0;nbbytes[TEL_FD]=0;

              if (cptfdstart == 0) {
                for (uint8_t rawcpt = cptfdstart; rawcpt < 2; rawcpt ++) {
                  len = 0;
                  switch ( dev[ rawcpt ].status ) {
                    case L1_ST:
        #if BOARD // On each iteration (1 sec), looking for a free channel
                      if (dev[ rawcpt ].fails == 0) {
                        dev[ rawcpt ].freescan ++;
                        if (dev[ rawcpt ].freescan == FREESCANPERIOD) {
                          dev[ rawcpt ].status = L2_ST ;
                          dev[ rawcpt ].freescan = 0;
                        }
                        lenlog += sprintf((char *)wfbmsg + lenlog, "Cpt(%d) status(%d) chan(%d) iter(%d)\n", \
                                        rawcpt, dev[ rawcpt ].status, dev[ rawcpt ].freqcptcur, dev[ rawcpt ].freescan);
                      } else {
                        dev[ rawcpt ].freescan = 0;
        #else  // On each iteration (2 sec), looking for channels receiving board messages (sync) 
                       if (true) {
        #endif // BOARD
                         dev[ rawcpt ].fails = 0;
                         // Change channel
                         if ( dev[ rawcpt ].freqcptcur < ( dev[ rawcpt ].freqsnb - 1 ) ) ( dev[ rawcpt ].freqcptcur) ++;
                         else dev[ rawcpt ].freqcptcur=0;
                         if (( cptfdstart == 0 ) && ( dev[ 0 ].freqcptcur == dev[ 1 ].freqcptcur)) {
                           if (dev[ rawcpt ].freqcptcur < ( dev[ rawcpt ].freqsnb - 1 ) ) ( dev[ rawcpt ].freqcptcur )++;
                           else dev[ rawcpt ].freqcptcur=0;
                         }
                         wfb_utils_setfreq( dev[ rawcpt ].freqcptcur,&dev[ rawcpt ] );
                         lenlog += sprintf((char *)wfbmsg + lenlog, "Cpt(%d) status(%d) Setreq chan(%d)\n", \
                                      rawcpt, dev[ rawcpt ].status, dev[ rawcpt ].freqcptcur);
                       }
                      break;
        #if BOARD
                    case L2_ST: // On each iteration (1 sec), check if in the time slot, the number of unknow messages do not exceed 
                      if (dev[ rawcpt ].fails > FREENBPKT) {
                        dev[ rawcpt ].freescan = 0;
                        if (cptbkup != -1) {
                          dev[ rawcpt ].status = L1_ST;
                          if (rawcpt == cptbkup) {
                            dev[ cptmain ].wfbdown.chan = -1;
                            cptbkup = -1;
                          }
                          if (rawcpt == cptmain) {
                            cptmain = cptbkup;
                            dev[ cptmain ].wfbdown.chan = -1;
                            cptbkup = -1;
                          }
                          lenlog += sprintf((char *)wfbmsg + lenlog, "Cpt(%d) switch cptmain(%d) cptbkup(%d)\n", \
                                      rawcpt, cptmain, cptbkup);
                        }
                      } else {
                        dev[ rawcpt ].freescan ++;
                        if (dev[ rawcpt ].freescan == FREETRANPERIOD) { dev[ rawcpt ].freescan = 0; dev[ rawcpt ].fails = 0; }
                        if (cptmain == -1) { cptmain = rawcpt; dev[ cptmain ].wfbdown.chan = -1; }
                        else {
                          if ((cptmain != rawcpt) && (cptbkup == -1)) {
                            cptbkup = rawcpt;
                            dev[ cptbkup ].wfbdown.chan = dev[ cptmain ].freqcptcur ;
                            dev[ cptmain ].wfbdown.chan = dev[ cptbkup ].freqcptcur ;
                          }
                        }
                        if (((cptmain >= 0) && (cptmain == rawcpt)) || ((cptbkup >= 0) && (cptbkup == rawcpt))) {
                          len = sizeof(wfbdown_t);
                          memcpy( &onlinebuff[ rawcpt ][ WFB_FD ][ offsetraw + sizeof(payhdr_t) + sizeof(subpayhdr_t) ],
                               &(dev[ rawcpt ].wfbdown), len);
                          ptr = &onlinebuff[ rawcpt ][ WFB_FD ][ offsetraw ];
                          (((payhdr_t *)ptr)->len) = len + sizeof(subpayhdr_t);
                          ptr += sizeof(payhdr_t);
                          (((subpayhdr_t *)ptr)->id) = WFB_FD;
                          (((subpayhdr_t *)ptr)->len) = len;
                          lentab[ rawcpt ][ WFB_FD ] = len;
                          dev[ rawcpt ].datatosend = true;
                        }
                      }
        
                      lenlog += sprintf((char *)wfbmsg + lenlog, "Cpt(%d) status(%d) chan(%d) iter(%d)\n", \
                                        rawcpt, dev[ rawcpt ].status, dev[ rawcpt ].freqcptcur, dev[ rawcpt ].freescan);
                      break;
        #endif // BOARD
                    default:
                      break;
                  } // switch
		} // for
              } // if cptfdstart
              break; // case TIME_FD

	    case RAW0_FD:
	    case RAW1_FD:
	      cpt -= 1;
#if RAW
              memset(& onlinebuff[ cpt ][ cpt ][ 0 ], 0, RADIOTAP_HEADER_MAX_SIZE + DRONEID_IEEE);
#else 
              memset(& onlinebuff[ cpt ][ cpt ][ 0 ], 0, sizeof(ieeehdr));
#endif // RAW 
              len = read( dev[ cpt ].fd, &onlinebuff[ cpt ][ cpt ][ 0 ], ONLINE_SIZE);
              if (len > 0) {
                dev[cpt].recvs++;

                lenlog += sprintf((char *)wfbmsg + lenlog, "Cpt(%d) read(%ld)\n",cpt,len);
#if RAW
                radiotapvar = (onlinebuff[cpt][cpt][2] + (onlinebuff[cpt][cpt][3] << 8)); // get variable radiotap header size
                offset_droneid = radiotapvar + DRONEID_IEEE;
                if ((offset_droneid < (RADIOTAP_HEADER_MAX_SIZE+DRONEID_IEEE))
                     &&(offset_droneid<len)&&(onlinebuff[cpt][cpt][offset_droneid] == DRONEID)) {
                  if (len>31) dev[cpt].antdbm = onlinebuff[cpt][cpt][31];
                  offset = radiotapvar + sizeof(ieeehdr);
                  datalen = sizeof(ieeehdr) + sizeof(payhdr_t) + ((payhdr_t *)(onlinebuff[cpt][cpt] + offset))->len;
                  if (datalen <= ONLINE_MTU) {
                    const uint8_t *s = &onlinebuff[cpt][cpt][radiotapvar];  // compute CRC32 after radiotap header
                    crc=0xFFFFFFFF;
                    for(uint32_t i=0;i<datalen;i++) {
                      uint8_t ch=s[i];
                      uint32_t t=(ch^crc)&0xFF;
                      crc=(crc>>8)^crc32_table[t];
                    }
                    memcpy(&crc_rx, &onlinebuff[cpt][cpt][len - 4], sizeof(crc_rx)); // CRC32 : last four bytes
                    if (~crc != crc_rx) {dev[cpt].fails ++;crcok=false;}
                    else crcok = true;
                  } else {dev[cpt].fails ++;crcok=false;}
                } else {dev[cpt].fails ++;crcok=false;}
#else
                if (onlinebuff[ cpt ][ cpt ][ DRONEID_IEEE ] == DRONEID) { offset = sizeof(ieeehdr); crcok = true; }
                else { dev[ cpt ].fails ++; crcok = false; }
#endif // RAW

#if BOARD
#else
#if RAW
                if (len >= (radiotapvar + FREQID_IEEE)) cptonline = onlinebuff[ cpt ][ cpt ][ radiotapvar + FREQID_IEEE ];
#else
                if (len >= FREQID_IEEE) cptonline = onlinebuff[ cpt ][ cpt ][ FREQID_IEEE ];
#endif // RAW
                if ((dev[ cpt ].cptreg >= 0) && (dev[ cpt ].cptreg != cptonline)) {
                  dev[ cpt ].status = L2_ST;
                  lenlog += sprintf((char *)wfbmsg + lenlog, "Cpt(%d) status(%d) Detect board switch (%d)\n",\
                                  cpt, dev[ cpt ].status,  dev[ cpt ].cptreg);
                }
#endif // BOARD
                ptr = &onlinebuff[ cpt ][ cpt ][ offset ];
                if (!crcok) { nbbytes_crcko += len; }
                else {
                  nbbytes_crcok += len;
                  //stp_n = ((payhdr_t *)ptr)->stp_n;
                  //seq = ((payhdr_t *)ptr)->seq;
                  //seq_prev = seq;
                  lensum = ((payhdr_t *)ptr)->len;
                  ptr += sizeof(payhdr_t);
                  while (lensum > 0) {
                    id = ((subpayhdr_t *)ptr)->id;
                    len = ((subpayhdr_t *)ptr)->len;
                    lensum -= (len + sizeof(subpayhdr_t));
                    ptr += sizeof(subpayhdr_t);
                    if (len > 0) {
#if BOARD
#else
                      if (id == WFB_FD) {
                        nbbytes[id] += len;
                        if (cptfdstart == 0) {
                          switch ( dev[ cpt ].status ) {
                            case L1_ST:
                              if (cptmain == -1) {
                                cptmain = cpt;
                                dev[ cpt ].status = L2_ST;
                                if (cptmain == 0) cptbkup = 1; else cptbkup = 0;
                                dev[ cptbkup ].status = L0_ST;
                                readlevel = cptmain;
                                lenlog += sprintf((char *)wfbmsg + lenlog, "Cpt(%d) status(%d) freq(%d) cptmainbkup(%d)(%d)\n", \
                                            cpt, dev[ cpt ].status, dev[ cpt ].freqcptcur,cptmain,cptbkup);
                              }
                              //No break, proceed with code below
  
                            case L2_ST:
                              if ((dev[ cptbkup ].status == L0_ST) && (((wfbdown_t *)ptr)->chan != -1)) {
                                dev[ cptbkup ].status = L2_ST;
                                readlevel = 2;
                                dev[ cptbkup ].freqcptcur = ((wfbdown_t *)ptr)->chan;
                                wfb_utils_setfreq( dev[ cptbkup ].freqcptcur,&dev[ cptbkup ] );
                              }
                              if ((cptmain == cpt) && (dev[ cptbkup ].status == L2_ST) && (((wfbdown_t *)ptr)->chan == -1)) {
                                dev[ cptbkup ].status = L0_ST;
                                readlevel = cptmain;
                              }
                              if ((cptbkup == cpt) && (((wfbdown_t *)ptr)->chan == -1)) {
                                cpttmp = cptbkup;
                                cptbkup = cptmain;
                                cptmain = cpttmp;
                                dev[ cptbkup ].status = L0_ST;
                                readlevel = cptmain;
                              }
                              lenlog += sprintf((char *)wfbmsg + lenlog, "Cpt(%d) status(%d) chan(%d) cptmainbkup(%d)(%d)\n", \
                                            cpt, dev[ cpt ].status, ((wfbdown_t *)ptr)->chan, cptmain,cptbkup);
                              break;
  
                            default:
                              break;
                          }
                        }
                      }
  #endif // BOARD   
                      if (id == TUN_FD) {
                        nbbytes[id] += len;
                        retwrite = write(dev[TUN_FD].fd, ptr, len);
                        lenlog += sprintf((char *)wfbmsg + lenlog, "TUN(%d) write(%d)(%ld)\n",dev[TUN_FD].fd,retwrite,len);
                      }
  #if TELEM
                      if (id == TEL_FD) {
                        nbbytes[id] += len;
  #if BOARD
                        retwrite = write(dev[TEL_FD].fd, ptr, len);
  #else
                        retwrite = sendto(dev[id].fd, ptr, len, 0, (struct sockaddr *)&(dev[ id ].addr_out[ 0 ]), sizeof(struct sockaddr));
  #endif // BOARD
                        lenlog += sprintf((char *)wfbmsg + lenlog, "TEL(%d) write(%d)(%ld)\n",dev[TEL_FD].fd,retwrite,len);
                      }
  #endif // TELEM
  #if BOARD
  #else
                      if ((id == VID1_FD) || (id == VID2_FD)) {
                        nbbytes[id] += len;
                        retwrite = sendto(dev[id].fd, ptr, len, 0, (struct sockaddr *)&(dev[ id ].addr_out[ 0 ]), sizeof(struct sockaddr));
                        lenlog += sprintf((char *)wfbmsg + lenlog, "VID(%d) write(%d)(%ld)\n",dev[id].fd,retwrite,len);
                      }
  #endif // BOARD   
                    }
                  }
                }
              }
	      break;

	    case TUN_FD:
#if BOARD
	    case VID1_FD:
	    case VID2_FD:
#endif // BOARD
#if TELEM
	    case TEL_FD:
#endif // TELEM
              len = read( dev[ cpt ].fd, &onlinebuff[ cptmain ][ cpt ][ offsetraw + sizeof(payhdr_t) + sizeof(subpayhdr_t)], ONLINE_SIZE);
              if (len > 0) {
                dev[cpt].recvs++;
                lenlog += sprintf((char *)wfbmsg + lenlog, "Cpt(%d) read(%ld) cptmain(%d)\n",cpt,len,cptmain);
  
                ptr = &onlinebuff[ cptmain ][ cpt ][ offsetraw ];
                (((payhdr_t *)ptr)->len) = len + sizeof(subpayhdr_t);;
                ptr += sizeof(payhdr_t);
                (((subpayhdr_t *)ptr)->id) = cpt;
                (((subpayhdr_t *)ptr)->len) = len;
                lentab[ cptmain ][ cpt ] = len;
                dev[ cptmain ].datatosend = true;
	      }
	      break;

            default:
              break;
	  }
        }
      }
    } // end poll
      
    for (uint8_t rawcpt = cptfdstart; rawcpt < 2; rawcpt ++) {
      if(dev[ rawcpt ].datatosend) {
        dev[ rawcpt ].datatosend=false;
        for (uint8_t cpt = (cptfdstart + 1); cpt < FD_NB; cpt ++) {
          if (lentab[ rawcpt ][ cpt ]!=0) {
            for (int i = cpt+1; i < FD_NB; i ++) {
              if (lentab[ rawcpt ][ i ] != 0) { // join packets to send whithin payload size 
                if (lentab[ rawcpt ][ cpt ] + lentab[ rawcpt ][ i ] < ( ONLINE_MTU - sizeof(subpayhdr_t))) {
                  if (lentab[ rawcpt ][ cpt ] > lentab[ rawcpt ][ i ]) { dst = cpt; src = i; } else { dst = i; src = cpt; }
                  memcpy(
                       &onlinebuff[ rawcpt ][ dst ][ offsetraw + sizeof(payhdr_t) + sizeof(subpayhdr_t) + lentab[ rawcpt ][ dst ]],
                       &onlinebuff[ rawcpt ][ src ][ offsetraw + sizeof(payhdr_t) ],
                       sizeof(subpayhdr_t) + lentab[ rawcpt ][ src ]);
                  ptr = &onlinebuff[ rawcpt ][ dst ][ offsetraw ];
                  (((payhdr_t *)ptr)->len) += (lentab[ rawcpt ][ src ] + sizeof(subpayhdr_t));
                  lentab[ rawcpt ][ src ]=0;
                }
              }
            }
            if (lentab[ rawcpt ][ cpt ] != 0) { // make sure current packet have not been joined
              ptr = &onlinebuff[ rawcpt ][ cpt ][ offsetraw ];
              (((payhdr_t *)ptr)->seq) = dev[ rawcpt ].seq_out;
//              (((payhdr_t *)ptr)->stp_n) = timecurr_us;
              len = (((payhdr_t *)ptr)->len);
              ieeehdr[ DRONEID_IEEE ] = DRONEID;
#if BOARD
              ieeehdr[ FREQID_IEEE ] = rawcpt;
#endif // BOARD
#if RAW
              memcpy(&onlinebuff[ rawcpt ][ cpt ][ 0 ], radiotaphdr, sizeof(radiotaphdr));
              memcpy(&onlinebuff[ rawcpt ][ cpt ][ 0 ] + sizeof(radiotaphdr), ieeehdr, sizeof(ieeehdr));
              len = write(dev[ rawcpt ].fd, &onlinebuff[ rawcpt ][ cpt ][ 0 ], offsetraw + sizeof(payhdr_t) + len);
#else
              memcpy(&onlinebuff[ rawcpt ][ cpt ][ 0 ], ieeehdr, sizeof(ieeehdr));
              len = sendto(dev[ rawcpt ].fd, &onlinebuff[ rawcpt ][ cpt ][ 0 ], offsetraw + sizeof(payhdr_t) + len, 0,
                     (struct sockaddr *)&(dev[ rawcpt ].addr_out[ rawcpt ]), sizeof(struct sockaddr));

#endif // RAW
              lenlog += sprintf((char *)wfbmsg + lenlog, "Cpt(%d) send(%ld)\n",rawcpt,len);

              lentab[ rawcpt ][ cpt ]=0;
              (dev[ rawcpt ].seq_out)++;
	    }
	  }
        }
      }
    }

    if (lenlog > 0) { 
      sendto(dev[ WFB_FD ].fd, wfbmsg, lenlog, 0, (struct sockaddr *)&(dev[ WFB_FD ].addr_out[ 0 ]), sizeof(struct sockaddr)); 
      lenlog=0;
    }
  }
}
