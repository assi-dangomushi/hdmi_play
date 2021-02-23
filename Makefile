OBJS=hdmi_play2.o
BIN=hdmi_play2.bin
LDFLAGS+=-lilclient

include /opt/vc/src/hello_pi/Makefile.include
CFLAGS+=-O2 -march=native

