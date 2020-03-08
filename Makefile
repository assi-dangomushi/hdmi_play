OBJS=hdmi_play.o
BIN=hdmi_play.bin
LDFLAGS+=-lilclient

include /opt/vc/src/hello_pi/Makefile.include
CFLAGS+=-O2 -march=native

