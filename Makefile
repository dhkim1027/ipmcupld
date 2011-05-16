SRCS = IPMCUPLD.c IpmiHpmFwUpg.c Serial.c WorkSet.c IPMI.c Md5.c Terminal.c
OBJS = $(SRCS:.c=.o)

TARGET = IPMCUPLD

INC_FILE =
INC_DIR = 

LIBS =

CC = gcc
CFLAGS += -W -Wall -D__LINUX__
CPPFLAGS += $(INC_DIR) $(INC_FILE)
LDFLAGS += -lpthread

all : $(TARGET)
$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.o : %.c
	@echo compiling... $<
	$(CC) $(CFLAGS) $(CPPFLAGS) -c  $<

clean:
	rm -rf $(OBJS) $(TARGET)
