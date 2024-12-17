#ROLE := BOARD
ROLE := GROUND

#OPT_TELEM := TELEM
OPT_TELEM :=

PROT := RAW
#PROT :=

ROLEFLAG :=
  ifeq ($(ROLE),BOARD)
    ROLEFLAG = -DBOARD=1
 else ifeq ($(ROLE),GROUND)
    ROLEFLAG = -DBOARD=0
 endif

PROTFLAG :=
  ifeq ($(PROT),RAW)
    PROTFLAG += -DRAW=1
    PROTFLAG += -DDRIVERNAME=\"rtl88XXau\"
#    PROTFLAG += -DDRIVERNAME=\"rtl88xxau_wfb\"
  else
    PROTFLAG += -DRAW=0
#    PROTFLAG += -DADDR_LOCAL_RAW=\"10.33.38.113\"
#    PROTFLAG += -DADDR_REMOTE_RAW=\"10.33.38.74\"
    PROTFLAG += -DADDR_LOCAL_RAW=\"192.168.3.1\"
    PROTFLAG += -DADDR_REMOTE_RAW=\"192.168.3.2\"
#   PROTFLAG += -DADDR_LOCAL_RAW=\"192.168.1.40\"
#   PROTFLAG += -DADDR_REMOTE_RAW=\"192.168.1.29\"
  endif

OSFLAG :=
  ifeq ($(OPT_TELEM),TELEM)
    OSFLAG += -DTELEM=1
    UNAME_R := $(shell uname -r)
    ifeq ($(UNAME_R),5.10.160-26-rk356x)
      OSFLAG += -DUART=\"/dev/ttyS4\"
    endif
  else
    OSFLAG += -DTELEM=0
  endif


TARGET   = wfb

#CFLAGS ?= -O2 -g
CFLAGS ?= -Ofast -g
CFLAGS += -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration
CFLAGS += -DCONFIG_LIBNL30 -I/usr/include/libnl3
CFLAGS += $(PROTFLAG) $(ROLEFLAG) $(OSFLAG)

LFLAGS  = -g
LFLAGS +=  -lnl-route-3 -lnl-genl-3 -lnl-3



SRCDIR   = src
OBJDIR   = obj
BINDIR   = bin

SOURCES  := $(wildcard $(SRCDIR)/*.c)
INCLUDES := $(wildcard $(SRCDIR)/*.h)
OBJECTS  := $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)


$(BINDIR)/$(TARGET): $(OBJECTS)
	gcc $(OBJECTS) $(LFLAGS) -o $@

$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.c
	gcc $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(BINDIR)/$(TARGET)
