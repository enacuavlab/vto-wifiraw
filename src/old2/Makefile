#ROLE := BOARD
ROLE := GROUND

#OPT_TELEM := TELEM
OPT_TELEM :=

#OPT_FEC := FEC
OPT_FEC :=

PROT := RAW
#PROT :=

INITCHAN0 := 0
INITCHAN1 := 30

all = wfb
OBJS = wfb.o wfb_utils.o

# DO NOT TOUCH BELOW THIS LINE
# ----------------------------
ROLEFLAG :=
  ifeq ($(ROLE),BOARD)
    ROLEFLAG = -DBOARD=1
 else ifeq ($(ROLE),GROUND)
    ROLEFLAG = -DBOARD=0
 endif

PROTFLAG :=
  ifeq ($(PROT),RAW)
    PROTFLAG += -DRAW=1
  else
    PROTFLAG += -DRAW=0
#    PROTFLAG += -DADDR_LOCAL_NET=\"192.168.3.1\"
#    PROTFLAG += -DADDR_REMOTE_NET=\"192.168.3.2\"
    PROTFLAG += -DADDR_LOCAL_NET=\"10.33.38.113\"
    PROTFLAG += -DADDR_REMOTE_NET=\"10.33.38.74\"
#    PROTFLAG += -DADDR_LOCAL_NET=\"192.168.1.131\"
#    PROTFLAG += -DADDR_REMOTE_NET=\"192.168.1.25\"
    PROTFLAG += -DUDP_RAW_PORT_START=3000
  endif

OSFLAG :=
  ifeq ($(OPT_TELEM),TELEM)
    OSFLAG += -DTELEM=1
    UNAME_R := $(shell uname -r)
    ifeq ($(UNAME_R),5.10.160-26-rk356x)
      OSFLAG += -DUART=\"/dev/ttyS4\"
    else ifeq ($(UNAME_R),6.1.0-1025-rockchip)
      OSFLAG += -DUART=\"/dev/ttyS4\"
    else ifeq ($(UNAME_R),5.10.160-legacy-rk35xx)
      OSFLAG += -DUART=\"/dev/ttyS7\"
    else ifeq ($(UNAME_R),4.9.253-tegra)
      OSFLAG += -DUART=\"/dev/ttyTHS1\"
    else ifeq ($(UNAME_R),6.1.21-v8+)
      OSFLAG += -DUART=\"/dev/ttyAMA0\"
    endif
  else 
    OSFLAG += -DTELEM=0
  endif

FECFLAG :=
  ifeq ($(OPT_FEC),FEC)
    FECFLAG += -DFEC=1
    OBJS += fec.o
  else 
    OSFLAG += -DFEC=0
  endif

CHANFLAG := -DINITCHAN0=$(INITCHAN0) -DINITCHAN1=$(INITCHAN1)

CFLAGS ?= -O2 -g
CFLAGS += -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration
CFLAGS += -DCONFIG_LIBNL30 -I/usr/include/libnl3
CFLAGS += $(PROTFLAG) $(ROLEFLAG) $(CHANFLAG) $(OSFLAG) $(FECFLAG)

LIBS +=  -lnl-route-3 -lnl-genl-3 -lnl-3

%.o: %.c
	gcc $(CFLAGS) -c -o $@ $<

wfb:	$(OBJS)
	gcc $(OBJS) $(LIBS) -g  -o wfb

clean:
	$(Q)rm -f *.o wfb
