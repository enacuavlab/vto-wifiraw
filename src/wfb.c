#include <stdlib.h>
#include <fcntl.h>

#include <time.h>
#include <sys/ioctl.h>
#include <net/ethernet.h>

#include <linux/filter.h>
#include <linux/if_packet.h>

#include <errno.h>
#include <sys/select.h>
#include <arpa/inet.h>

#include "wfb_utils.h"
#include "wfb.h"

#define GET_TEMPERATURE fflush(tempstream);fseek(tempstream,0,SEEK_SET);fread(&strtmp,1,sizeof(strtmp),tempstream);wfbdown.temp = atoi(strtmp);

/*****************************************************************************/
int main(int argc, char **argv) {

  FILE *tempstream=fopen("/sys/class/thermal/thermal_zone0/temp","rb");
  bool switchonce=true,inverted=false,backuprefresh=true,backuponce=true;
  int8_t cptmain=-1,cptbackup=-1,cpttmp=-1;
  uint8_t raw0_cptfreq=0,raw1_cptfreq=18;
  uint8_t cptfdstart=0,listenflag=0;
  wfb_utils_t dev[2];
  if (wfb_utils_init(&dev[1])) { // RAW1_FD
    cptfdstart = 1;
    listenflag = 1;
    dev[1].wfbdown.mainchan = false;
    dev[1].wfbdown.backupchan = -2;
    dev[1].wfbup.backupchan = -2;
    if (raw1_cptfreq < dev[1].freqsnb) {
      if (wfb_utils_setfreq(raw1_cptfreq,&dev[1])) {
        if (wfb_utils_init(&dev[0])) { // RAW0_FD
          cptfdstart = 0;
	  listenflag = 2;
	  dev[0].wfbdown.mainchan = false;
	  dev[1].wfbdown.mainchan = false;
	  dev[0].wfbdown.backupchan = -1;
	  dev[1].wfbdown.backupchan = -1;
	  dev[0].wfbup.backupchan = -1;
	  dev[1].wfbup.backupchan = -1;
          if (raw0_cptfreq < dev[0].freqsnb) {
            if (!wfb_utils_setfreq(raw0_cptfreq,&dev[0])) exit(-1);
          }
        }
      }
    }
  } else exit(-1);
  init_t param;
  if (!wfb_init(dev[0].name,dev[1].name,&param)) exit(-1);

#ifdef RAW
  uint16_t datalen,radiotapvar,offset_droneid;
  uint8_t offset, droneid_char='1'-DRONEID;
  uint32_t crc, crc_rx;
#endif // RAW
  struct timeval timeout;
  struct timespec stp;
  bool crcok=false,timetosend=false;
  uint8_t onlinebuff[2][FD_NB][ONLINE_SIZE],*ptr,id;

  uint16_t dst,src,seq_prev,seq;
  int ret;
  ssize_t len,lensum;

  // should be in dev struct
  ssize_t lentab[2][FD_NB];
  memset(&lentab,0, 2*FD_NB*sizeof(ssize_t));

  uint8_t cptfdend;
  uint16_t maxfd,fdtmp;
  fd_set readset;


#if ROLE
  uint64_t delay_n=1000000000L;
#else
  uint64_t delay_n=2000000000L;
#endif  // ROLE
  uint8_t delay_s=(delay_n / 1000000000L);
  uint64_t stp_n=0,stp_n1=0,stp_n2=0,delaytot_n=0;

  bool debugonce=true;

  for(;;) {
    FD_ZERO(&readset);
    if (cptmain>=0) {readset = param.readset_all[listenflag]; maxfd = param.maxfd_all[listenflag]; cptfdend = FD_NB;}
    else {readset = param.readset_raw[listenflag]; maxfd = param.maxfd_raw[listenflag]; cptfdend = (1+WFB_FD);}
    timeout.tv_sec = delay_s; timeout.tv_usec = 0;

#if ROLE
#else
    if(((cptfdstart==0)&&(cptbackup>0)&&(dev[0].repeat==0)&&(dev[1].repeat==0))||((cptfdstart==1)&&(cptmain>=0)&&(dev[1].repeat==0))) 
      ret = select(maxfd + 1, &readset, NULL, NULL, NULL);
    else
#endif  // ROLE
      ret = select(maxfd + 1, &readset, NULL, NULL, &timeout);
    for (uint8_t cpt = cptfdstart; cpt < cptfdend; cpt++) {
      if (cpt<=RAW1_FD) {
	if (FD_ISSET(param.fd[cpt], &readset)) { 
// Read incomming data from active channel and optional backup channel
          len = read(param.fd[cpt], &onlinebuff[cpt][cpt][0], ONLINE_SIZE);
#if ROLE
          if (len>0) {
#else
          if ((len>9) && (0!=memcmp(&onlinebuff[cpt][cpt][DRONIDSSIDPOS_RX],&droneidbuf[DRONIDSSIDPOS_TX],9))) {
#endif // ROLE
#ifdef RAW
            radiotapvar = (onlinebuff[cpt][cpt][2] + (onlinebuff[cpt][cpt][3] << 8)); // get variable radiotap header size
            offset_droneid =radiotapvar + DRONEID_IEEE;									  
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
                ptr=&onlinebuff[cpt][cpt][0]+offset;
              } else {dev[cpt].fails ++;crcok=false;}
            } else {dev[cpt].fails ++;crcok=false;}
#else
            ptr=&onlinebuff[cpt][cpt][0];
            crcok = true;
#endif // RAW
            if (crcok) {
              stp_n = ((payhdr_t *)ptr)->stp_n;
              seq = ((payhdr_t *)ptr)->seq;
              seq_prev = seq;
              lensum = ((payhdr_t *)ptr)->len;
              ptr+=sizeof(payhdr_t);
              while (lensum>0) {
                id = ((subpayhdr_t *)ptr)->id;
                len = ((subpayhdr_t *)ptr)->len;
                lensum -= (len + sizeof(subpayhdr_t));
                ptr+=sizeof(subpayhdr_t);
		if (cptmain>=0) {
                  if(id==TUN_FD) {
		    write(param.fd[id], ptr, len);
		  } 
                  else lensum=0;
		}
                if (id==WFB_FD) {
#if ROLE
		  if (cptmain>=0) {
		    if ((((wfbup_t *)ptr)->backupchan)==-2) {
		      cptmain=cpt;
                      if (cptmain==0)cpttmp=1;else cpttmp=0;
                      dev[cpttmp].sync_active=true;
		      listenflag=cptmain;
		    }
                  }
#else
		  if ((cptmain<0)&&(cptfdstart==1)) {
                      cptmain=cpt;
		      dev[cptmain].unlockfreq=false;
		      dev[cptmain].wfbup.backupchan=-2;
                      dev[cptmain].repeat=2;
		      if(cptmain==0)cptbackup=1;else cptbackup=0;
                      dev[cptbackup].sync_active = true;
		      printf("(%d)\n",cptmain);

		  }
                  if (cptfdstart==0) {
                    if (cptmain<0) {
                      if (((wfbdown_t *)ptr)->mainchan) {
		        cptmain=cpt;
                        dev[cptmain].unlockfreq=false;
		        if ((((wfbdown_t *)ptr)->backupchan)>=0) {
		          if(cptmain==0)cptbackup=1;else cptbackup=0;
                          dev[cptbackup].unlockfreq=false;
                          dev[cptbackup].freqcptcur=(((wfbdown_t *)ptr)->backupchan);
		          wfb_utils_setfreq(dev[cptbackup].freqcptcur,&dev[cptbackup]);
		        } 
                        if ((((wfbdown_t *)ptr)->backupchan)==-1) {
                          listenflag=cptmain;
			}
                        if ((((wfbdown_t *)ptr)->backupchan)==-2) {
		          dev[cptmain].wfbup.backupchan=-2;
                          dev[cptmain].repeat=2;
			  listenflag=cptmain;
                          if(cptmain==0)cptbackup=1;else cptbackup=0;
                          dev[cptbackup].sync_active = true;
                        }
                      } else {
  		        cptbackup=cpt;
  		        dev[cptbackup].unlockfreq=false;
  		        if(cptbackup==0)cptmain=1;else cptmain=0;
                        dev[cptmain].unlockfreq=false;
                        dev[cptmain].freqcptcur=(((wfbdown_t *)ptr)->backupchan);
  		        wfb_utils_setfreq(dev[cptmain].freqcptcur,&dev[cptmain]);
		      }
                    } else { // else if (cptmain<0)
                      if((cptbackup<0)&&(((wfbdown_t *)ptr)->backupchan>=0)) {
		        if(cptmain==0)cptbackup=1;else cptbackup=0;
                        dev[cptbackup].unlockfreq=false;
		        dev[cptbackup].freqcptcur=(((wfbdown_t *)ptr)->backupchan);
                        wfb_utils_setfreq(dev[cptbackup].freqcptcur,&dev[cptbackup]);
		        listenflag=2;
		      }
                    } // end if (cptmain<0)
                    if ((cptmain>=0)&&(cptbackup>=0)&&((((wfbdown_t *)ptr)->mainchan)==true)) {
		      if (cpt==cptbackup) {
  		        if ((((wfbdown_t *)ptr)->backupchan)<0) {
		          if (switchonce) {
                            switchonce=false;
			    inverted=true;
			    cpttmp=cptmain;cptmain=cptbackup;cptbackup=cpttmp;
			  }
			}
		      }
		      if ((inverted)&&(!switchonce)&&(cpt==cptmain)&&((((wfbdown_t *)ptr)->backupchan)>=0)) {
                        inverted=false;
                        switchonce=true;
                        dev[cptbackup].unlockfreq=false;
		        dev[cptbackup].freqcptcur=(((wfbdown_t *)ptr)->backupchan);
                        wfb_utils_setfreq(dev[cptbackup].freqcptcur,&dev[cptbackup]);
		      }
		    }
                    if ((cptmain>=0)&&(cptbackup>=0)&&((((wfbdown_t *)ptr)->mainchan)==true)) {
                      if ((backuprefresh)&&((((wfbdown_t *)ptr)->backupchan)==-3)) {
		        backuprefresh=false;
//			listenflag=cptmain;
			printf("REFRESHING\n");
		      }
                      if ((backuponce)&&(!backuprefresh)&&((((wfbdown_t *)ptr)->backupchan)>=0)) {
                        backuponce=false;
                        backuprefresh=true;
//			listenflag=2;
                        dev[cptbackup].unlockfreq=false;
		        dev[cptbackup].freqcptcur=(((wfbdown_t *)ptr)->backupchan);
                        wfb_utils_setfreq(dev[cptbackup].freqcptcur,&dev[cptbackup]);
                        printf("REFRESHED\n");
		      }
		    }
		  }
#endif // ROLE
                }
                ptr+=len;
              }
	    }
          }
        }
      } else {
        if (cpt==WFB_FD) {
	  clock_gettime( CLOCK_MONOTONIC, &stp);
          stp_n1 = (stp.tv_nsec + (stp.tv_sec * 1000000000L));
	  if (stp_n2!=0) {
	    delaytot_n += (stp_n1 - stp_n2);
	    if (delaytot_n>=delay_n) {delaytot_n=0;timetosend=true;}	
	  }
	  stp_n2 = stp_n1;
#if ROLE
// DEBUG CODE
          if ((debugonce)&&(cptbackup>=0)&&(timetosend)) 
	    printf("(%d)(%d)(%d)(%d)(%d)\n",cptmain,cptbackup,dev[cptmain].freqcptcur,dev[cptbackup].freqcptcur,dev[cptbackup].fails++);
// END DEBUG CODE
          if ((cptbackup>=0)&&(timetosend)) {
	    if (dev[cptmain].fails>60) {
              dev[cptbackup].sync_active = false;
	      dev[cptbackup].wfbdown.mainchan=true;
	      dev[cptbackup].wfbdown.backupchan=-1;

              dev[cptmain].sync_active = false;
              dev[cptmain].unlockfreq = true;
	      dev[cptmain].wfbdown.mainchan=false;

	      cptmain=cptbackup;
	      cptbackup=-1;
	    }
	    if ((debugonce)&&(dev[cptbackup].fails>60)) {
	      dev[cptmain].wfbdown.backupchan=-3;
              dev[cptbackup].sync_active = false;
              dev[cptbackup].unlockfreq = true;
	      cptbackup=-1;
	      // TODO
	    }
// DEBUG CODE
// END DEBUG CODE
	  } 
#endif // ROLE
          for (uint8_t rawcpt = cptfdstart; rawcpt < 2; rawcpt++) {
#if ROLE
            if (!(dev[rawcpt].sync_active)&&(timetosend)) {
              if (dev[rawcpt].unlockfreq) {
                if (dev[rawcpt].fails > 0) {
                  dev[rawcpt].fails = 0;
  		  dev[rawcpt].nofaultsec=0;
                  if (dev[rawcpt].freqcptcur<(dev[rawcpt].freqsnb-1))(dev[rawcpt].freqcptcur)++;else dev[rawcpt].freqcptcur=0;
		  if ((cptfdstart==0) &&(dev[0].freqcptcur==dev[1].freqcptcur)) {
                    if (dev[rawcpt].freqcptcur<(dev[rawcpt].freqsnb-1))(dev[rawcpt].freqcptcur)++;else dev[rawcpt].freqcptcur=0;
		  }
    		  wfb_utils_setfreq(dev[rawcpt].freqcptcur,&dev[rawcpt]);
  	        } else {
  		  if ((dev[rawcpt].nofaultsec++)>30)dev[rawcpt].unlockfreq=false;
  	        }
              } else {
		len = 0;
		if (cptmain<0) {cptmain=rawcpt;dev[cptmain].wfbdown.mainchan=true;}
		else if ((rawcpt!=cptmain)&&(cptbackup<0)&&(!(dev[rawcpt].unlockfreq))) {
		  cptbackup=rawcpt;
		  dev[cptmain].wfbdown.backupchan=dev[cptbackup].freqcptcur;
		  dev[cptbackup].wfbdown.backupchan=dev[cptmain].freqcptcur;
		}
	        if (((rawcpt==cptbackup)&&(cptbackup>=0))||((rawcpt==cptmain)&&(cptmain>=0))){
                  dev[rawcpt].wfbdown.chan=dev[rawcpt].freqcptcur;
		  memcpy(&onlinebuff[rawcpt][cpt][0]+(param.offsetraw)+sizeof(payhdr_t)+sizeof(subpayhdr_t), 
			 &dev[rawcpt].wfbdown, sizeof(wfbdown_t));
	          len = sizeof(wfbdown_t);
	        }
#else
            if (!(dev[rawcpt].sync_active)&&(timetosend)) {
	      if (dev[rawcpt].unlockfreq) {
                if (dev[rawcpt].freqcptcur < (dev[rawcpt].freqsnb-1)) (dev[rawcpt].freqcptcur)++; else dev[rawcpt].freqcptcur = 0;
		if ((cptfdstart==0)&&(dev[0].freqcptcur == dev[1].freqcptcur)){
                  if (dev[rawcpt].freqcptcur < (dev[rawcpt].freqsnb-1)) (dev[rawcpt].freqcptcur)++; else dev[rawcpt].freqcptcur = 0;
	        }
                wfb_utils_setfreq(dev[rawcpt].freqcptcur,&dev[rawcpt]);
              } else {
		len = 0;
		if (dev[rawcpt].repeat>0 ) { 
		  dev[rawcpt].repeat--;
                  memcpy(&onlinebuff[rawcpt][cpt][0]+(param.offsetraw)+sizeof(payhdr_t)+sizeof(subpayhdr_t),&dev[rawcpt].wfbup, sizeof(wfbup_t));
	          len = sizeof(wfbup_t);
		}
#endif // ROLE
                if (len!=0) {
                  ptr=&onlinebuff[rawcpt][cpt][0]+(param.offsetraw);
                  (((payhdr_t *)ptr)->len) = len + sizeof(subpayhdr_t);;
                  ptr+=sizeof(payhdr_t);
                  (((subpayhdr_t *)ptr)->id) = cpt;
                  (((subpayhdr_t *)ptr)->len) = len;
  	          lentab[rawcpt][cpt] = len;
  	          dev[rawcpt].datatosend=true;
		}
              }
            }
	  } // end for
	  timetosend=false;
	} else { // else WFB_FD
          if (cptmain>=0) {
            if ((param.fd[cpt]!=0)&&FD_ISSET(param.fd[cpt], &readset)) {
              len = read(param.fd[cpt],&onlinebuff[cptmain][cpt][0]+(param.offsetraw)+sizeof(payhdr_t)+sizeof(subpayhdr_t),
			  ONLINE_SIZE-(param.offsetraw)-sizeof(payhdr_t)-sizeof(subpayhdr_t));
              if (len>0) {
                ptr=&onlinebuff[cptmain][cpt][0]+(param.offsetraw);
                (((payhdr_t *)ptr)->len) = len + sizeof(subpayhdr_t);;
                ptr+=sizeof(payhdr_t);
                (((subpayhdr_t *)ptr)->id) = cpt;
                (((subpayhdr_t *)ptr)->len) = len;
                lentab[cptmain][cpt] = len;
                dev[cptmain].datatosend=true;
	      }
	    }
	  }
	}
      }
    } // end for
    for (uint8_t rawcpt = cptfdstart; rawcpt < 2; rawcpt++) {
      if(dev[rawcpt].datatosend) {
        dev[rawcpt].datatosend=false;
#if ROLE
	if ((rawcpt==cptmain)&&(lentab[rawcpt][WFB_FD]!=0)) write(param.fd[rawcpt],&droneidbuf,sizeof(droneidbuf));
#endif // ROLE
        for (uint8_t cpt = (cptfdstart+1); cpt < FD_NB; cpt++) {
          if (lentab[rawcpt][cpt]!=0) {
            for (int i=cpt+1;i<FD_NB;i++) {
              if (lentab[rawcpt][i]!=0) {
                if (lentab[rawcpt][cpt]+lentab[rawcpt][i] < (ONLINE_MTU-sizeof(subpayhdr_t))) { // join packets to send whithin payload size 
                  if (lentab[rawcpt][cpt]>lentab[rawcpt][i]) { dst=cpt; src=i; } else { dst=i; src=cpt; }
                  memcpy(&onlinebuff[rawcpt][dst][0]+(param.offsetraw)+sizeof(payhdr_t)+sizeof(subpayhdr_t)+lentab[rawcpt][dst],
                       &onlinebuff[rawcpt][src][0]+(param.offsetraw)+sizeof(payhdr_t),
                       sizeof(subpayhdr_t)+lentab[rawcpt][src]);
                  ptr=&onlinebuff[rawcpt][dst][0]+(param.offsetraw);
                  (((payhdr_t *)ptr)->len)  += (lentab[rawcpt][src]+sizeof(subpayhdr_t));
                  lentab[rawcpt][src]=0;
                }
              }
            }
            if (lentab[rawcpt][cpt]!=0) { // make sure current packet have not been joined
              ptr = &onlinebuff[rawcpt][cpt][0]+(param.offsetraw);
              (((payhdr_t *)ptr)->seq) = dev[rawcpt].seq_out;
              (((payhdr_t *)ptr)->stp_n) = stp_n;
              len = (((payhdr_t *)ptr)->len);
#ifdef RAW
              memcpy(&onlinebuff[rawcpt][cpt][0],radiotaphdr,sizeof(radiotaphdr));
              memcpy(&onlinebuff[rawcpt][cpt][0]+sizeof(radiotaphdr),ieeehdr,sizeof(ieeehdr));
              len = write(param.fd[rawcpt],&onlinebuff[rawcpt][cpt][0],(param.offsetraw)+sizeof(payhdr_t)+len);
#else
              len = sendto(param.fd[rawcwcpt],&onlinebuff[cpt][0]+(param.offsetraw),sizeof(payhdr_t)+len,0,
			   (struct sockaddr *)&(param.addr_out[0]), sizeof(struct sockaddr));
#endif // RAW

              lentab[rawcpt][cpt]=0;
              (dev[rawcpt].seq_out)++;
#if ROLE
              dev[rawcpt].wfbdown.sent=dev[rawcpt].seq_out;
#endif // ROLE
            }
          }
        }
      }
    }
  }
}
