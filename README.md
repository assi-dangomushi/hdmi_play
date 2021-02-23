# hdmi_play

## Raspberry pi　HDMI 8ch LPCM 96kHz 24bit output

Read data from stdin (8ch, 32bit) and output to HDMI

cd /opt/vc/src/hello_pi/libs/ilclient/

make

cd ~

git clone https://github.com/assi-dangomushi/hdmi_play.git

cd hdmi_play

make

## Usage

hdmi_play2.bin samplerate

Example: sox test.wav -t .s16 - | brutefir 3way.conf | hdmi_play2.bin 44100


### Notice:
ChannelMapping is changed (2021/02/23)
0 : LF
1 : RF
2 : LB
3 : RB
4 : CF
5 : LFE
6 : LS
7 : RS

You must connect a full-range unit and check channel mapping, befor using expensive units.
