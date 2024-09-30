#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#include "wfb.h"
#include "wfb_utils.h"

int main(void) {

  uint8_t cptfdstart = 0, cptfdend = 0;
  uint16_t maxfdraw[3] = {0,0,0}, maxfdall[3] = {0,0,0};
  fd_set readsetraw[3], readsetall[3];
  for (int i=0;i<3;i++) { FD_ZERO( &readsetraw[i] ); FD_ZERO( &readsetall[i] ); }
  wfb_utils_t dev[ FD_NB ];

  int8_t cptmain, cptbkup = -1, cpttmp;
  if (wfb_utils_init( 1, &dev[ 1 ])) { // RAW1_FD
    if (INITCHAN1 < dev[ 1 ].freqsnb) {
      if (wfb_utils_setfreq( INITCHAN1 , &dev[ 1 ] )) {
	printf("ONE [%s]\n", dev[ 1 ].name);
        FD_SET( dev[ 1 ].fd, &readsetraw[ 1 ] ); FD_SET( dev[ 1 ].fd, &readsetall[ 1 ] ); maxfdraw[1] = dev[ 1 ].fd;
        FD_SET( dev[ 1 ].fd, &readsetraw[ 2 ] ); FD_SET( dev[ 1 ].fd, &readsetall[ 2 ] ); maxfdraw[2] = dev[ 1 ].fd;
	cptfdstart = 1; cptmain = 1;
        if (wfb_utils_init( 0, &dev[ 0 ] )) { // RAW0_FD
	  if (INITCHAN0 < dev[ 0 ].freqsnb) {
            if (wfb_utils_setfreq( INITCHAN0, &dev[ 0 ] )) {
	      printf("TWO [%s]\n", dev[ 0 ].name);
	      FD_SET( dev[ 0 ].fd, &readsetraw[ 0 ] ); FD_SET( dev[ 0 ].fd, &readsetall[ 0 ] ); maxfdraw[0] = dev[ 0 ].fd;
	      FD_SET( dev[ 0 ].fd, &readsetraw[ 2 ] ); FD_SET( dev[ 0 ].fd, &readsetall[ 2 ] );
              if (( dev[ 0 ].fd) > maxfdraw[ 2 ]) maxfdraw[ 2 ] = dev[ 0 ].fd;
	      cptfdstart = 0; cptmain = -1;
	    } else exit(-1);
	  } else exit(-1);
	} 
      } else exit(-1);  
    } else exit(-1); 
  } else exit(-1); 

  for (int i = cptfdstart; i < 3; i++) maxfdall[i] = maxfdraw[i];
  wfb_addusr( dev, cptfdstart, maxfdall, readsetall);

  int64_t time_us,timecurr_us,elapsed_us;
  struct timespec stp;        // tv_sec:long, tv_nsec:long (nanosecs x 1000000000L))
  struct timeval timeoutleft; // tv_sec:long, tv_usec: (microsec x 1000000L)

  clock_gettime( CLOCK_MONOTONIC, &stp);
  timecurr_us = (int64_t)stp.tv_sec * 1000000L + (int64_t)stp.tv_nsec / 1000;

#define FREESCANPERIOD 20
#if BOARD
#define FREETRANPERIOD 5
#define FREENBPKT 2
#define PERIOD_DELAY_US 1000000L // Must be greater than 1000 to be in timeout select
#else
#define PERIOD_DELAY_US 2000000L // Must be greater than 1000 to be in timeout select
#endif // BOARD
       
  time_us = timecurr_us + PERIOD_DELAY_US;;
  timeoutleft.tv_sec = PERIOD_DELAY_US / 1000000L; timeoutleft.tv_usec = PERIOD_DELAY_US % 1000000L;

  bool expired = false, crcok = false;
  int16_t retselect, retwrite;
  uint8_t onlinebuff[ 2 ][ FD_NB ][ ONLINE_SIZE ], *ptr, id, lenlog=0, wfbmsg[300], cptonline, readlevel = 2;
  uint16_t offset, dst, src, maxfd;
  uint16_t nbits[FD_NB] = {0}, nbits_crcko = 0, nbits_crcok = 0;
  fd_set readset;

  ssize_t lensum, len = 0, lentab[ 2 ][ FD_NB ];
  memset(&lentab,0, 2 * FD_NB * sizeof(ssize_t));

  uint16_t offsetraw = sizeof(ieeehdr);
#if RAW
  offsetraw += sizeof(radiotaphdr);
  extern uint32_t crc32_table[256];
  uint8_t droneid_char='1'-DRONEID;
  uint16_t radiotapvar, offset_droneid=0, datalen;
  uint32_t crc, crc_rx;
  extern uint32_t crc32_table[256];
#endif // RAW 

  for(;;) {

    if (cptmain == -1) { readset = readsetraw[ readlevel ]; maxfd = maxfdraw[ readlevel ]; cptfdend = (1 + WFB_FD); }
    else { readset = readsetall[ readlevel ]; maxfd = maxfdall[ readlevel ]; cptfdend = FD_NB; }

    int8_t retselect = select(maxfd + 1, &readset, NULL, NULL, &timeoutleft);
    if ( retselect != 0 ) {
      for (uint8_t cpt = cptfdstart; cpt < cptfdend; cpt++) { // receive
        if (FD_ISSET( dev[ cpt ].fd, &readset)) {
          if (cpt <= RAW1_FD) {
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
                   &&(offset_droneid<len)&&((*onlinebuff[cpt][cpt] + offset_droneid) == droneid_char)) {
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
              if (!crcok) { nbits_crcko += len; }
	      else {
		nbits_crcok += len;
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
		      nbits[id] += len;
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
		      nbits[id] += len;
		      retwrite = write(dev[TUN_FD].fd, ptr, len);
	              lenlog += sprintf((char *)wfbmsg + lenlog, "TUN(%d) write(%d)(%ld)\n",dev[TUN_FD].fd,retwrite,len);
		    }
#if TELEM
  		    if (id == TEL_FD) {
                      nbits[id] += len;
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
                      nbits[id] += len;
  		      retwrite = sendto(dev[id].fd, ptr, len, 0, (struct sockaddr *)&(dev[ id ].addr_out[ 0 ]), sizeof(struct sockaddr));
	              lenlog += sprintf((char *)wfbmsg + lenlog, "VID(%d) write(%d)(%ld)\n",dev[id].fd,retwrite,len);
		    }
#endif // BOARD   
		  }
		}
              }
	    }
	  } else { // else RAW1_FD
            len = read( dev[ cpt ].fd, &onlinebuff[ cptmain ][ cpt ][ offsetraw + sizeof(payhdr_t) + sizeof(subpayhdr_t)], 
			ONLINE_SIZE);
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
	  }
	}
      }
    } // ret	  
      
    clock_gettime( CLOCK_MONOTONIC, &stp);
    timecurr_us = (int64_t)stp.tv_sec * 1000000L + (int64_t)stp.tv_nsec / 1000;
    elapsed_us = time_us - timecurr_us;
    if ((elapsed_us < 0) || (elapsed_us < 1000)) 
      {time_us = timecurr_us + PERIOD_DELAY_US; expired = true; elapsed_us = PERIOD_DELAY_US;}
    timeoutleft.tv_sec = elapsed_us / 1000000L; timeoutleft.tv_usec = elapsed_us % 1000000L;

    if ( expired ) {
      expired = false;

#if BOARD
#else
      nbits_crcok/=2; nbits_crcko/=2; nbits[WFB_FD]/=2; nbits[TUN_FD]/=2; nbits[VID1_FD]/=2; nbits[VID2_FD]/=2; nbits[TEL_FD]/=2;
#endif // BOARD
 
      lenlog += sprintf((char *)wfbmsg + lenlog, "Bitrates OK(%d) KO(%d) WFB(%d) TUN(%d) VID1(%d) VID2(%d) TEL(%d)\n", \
        nbits_crcok, nbits_crcko, nbits[WFB_FD], nbits[TUN_FD], nbits[VID1_FD], nbits[VID2_FD], nbits[TEL_FD]);
      nbits_crcok=0; nbits_crcko=0; nbits[WFB_FD]=0; nbits[TUN_FD]=0; nbits[VID1_FD]=0; nbits[VID2_FD]=0; nbits[TEL_FD]=0;

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
          }
        } // for
      }
    } // expired

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
                (((payhdr_t *)ptr)->stp_n) = timecurr_us;
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
