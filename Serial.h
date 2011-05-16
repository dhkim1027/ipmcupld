#ifndef __SERIAL_H__
#define __SERIAL_H__

#ifdef __LINUX__
#include <stdbool.h>
#endif

#ifndef bool
typedef unsigned char bool;
#endif

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif


bool SerialOpen(char* port, int baudrate);
int SerialReadByte(void *dummy);
unsigned char SerialReadByte2(void);
int SerialReadData(char *buf, int size);
int SerialWriteByte(char byte);
int SerialWriteData(char *buf, int size);
int SerialClose(void);

#endif
