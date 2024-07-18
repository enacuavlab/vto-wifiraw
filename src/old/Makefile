#ROLE := BOARD
ROLE := GROUND

#OPT_TELEM := TELEM
OPT_TELEM :=

PROT := RAW
#PROT :=

INITCHAN0 := 0
INITCHAN1 := 30


# DO NOT TOUCH BELOW THIS LINE
# ----------------------------
ROLEFLAG :=
  ifeq ($(ROLE),BOARD)
    ROLEFLAG = -DROLE=1
 else ifeq ($(ROLE),GROUND)
    ROLEFLAG = -DROLE=0
 endif

PROTFLAG :=
  ifeq ($(PROT),RAW)
    PROTFLAG += -DRAW=1
  else
    PROTFLAG += -DRAW=0
    PROTFLAG += -DADDR_REMOTE_GROUND=\"10.33.38.74\"
#    PROTFLAG += -DADDR_REMOTE_BOARD=\"192.168.1.19\"
#    PROTFLAG += -DADDR_REMOTE_GROUND=\"192.168.1.19\"
    PROTFLAG += -DADDR_REMOTE_BOARD=\"10.33.38.113\"
  endif

OSFLAG :=
  ifeq ($(OPT_TELEM),TELEM)
    OSFLAG += -DOPT_TELEM=1
    UNAME_R := $(shell uname -r)
    ifeq ($(UNAME_R),5.10.160-legacy-rk35xx)
      OSFLAG = -DUART=\"/dev/ttyS7\"
    else ifeq ($(UNAME_R),4.9.253-tegra)
      OSFLAG = -DUART=\"/dev/ttyTHS1\"
    else ifeq ($(UNAME_R),6.1.21-v8+)
      OSFLAG = -DUART=\"/dev/ttyAMA0\"
    endif
  else 
    OSFLAG += -DOPT_TELEM=0
  endif

CHANFLAG := -DINITCHAN0=$(INITCHAN0) -DINITCHAN1=$(INITCHAN1)

all = wfb
OBJS = wfb.o wfb_utils.o

CFLAGS ?= -O2 -g
CFLAGS += -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration
CFLAGS += -DCONFIG_LIBNL30 -I/usr/include/libnl3
CFLAGS += $(PROTFLAG) $(ROLEFLAG) $(CHANFLAG) $(OSFLAG)

LIBS +=  -lnl-route-3 -lnl-genl-3 -lnl-3

%.o: %.c
	gcc $(CFLAGS) -c -o $@ $<

wfb:	$(OBJS)
	gcc $(OBJS) $(LIBS) -g  -o wfb

clean:
	$(Q)rm -f wfb *.o
