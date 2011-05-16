#ifdef __WIN32__
#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "WorkSet.h"
#include "Terminal.h"
#include "Serial.h"

#ifdef __WIN32__
HANDLE	hFile = INVALID_HANDLE_VALUE;		
DCB		oldPortOpt;
#endif

int		nPortOpen = 0;
WS_t	*ws = 0;

bool 
SerialOpen(char* port, int baudrate)
{
#ifdef __WIN32__
	DCB				newPortOpt;
	COMMTIMEOUTS	cTimeout;
	
	// ����Ʈ ����
	hFile = CreateFile(port,
				GENERIC_READ | GENERIC_WRITE,
				0,				// �����
				0,				// ��ť��Ƽ �Ӽ�:������
				OPEN_EXISTING,	// ���� ���� ����
				FILE_FLAG_WRITE_THROUGH | FILE_FLAG_NO_BUFFERING,
				//FILE_ATTRIBUTE_NORMAL, 
				0 );			// �Ӽ�, ���÷���Ʈ
		
	if(hFile == INVALID_HANDLE_VALUE){
		printf("error : open port %s\n", port);
		return false;
	}
			
	// ������ ��� ����̽��� ���� DCB ���� ���
	// DCB : Device Control Block ����̽� ���� ��
	GetCommState(hFile , &oldPortOpt);		
	memcpy(&newPortOpt, &oldPortOpt, sizeof(DCB));
	newPortOpt.BaudRate = baudrate;
	SetCommState(hFile , &newPortOpt);			

	// ���� �������� Ÿ�Ӿƿ� �ڷ� ���
	//GetCommTimeouts(hFile, &cTimeout);	

	cTimeout.ReadIntervalTimeout         = 1000;	
	cTimeout.ReadTotalTimeoutMultiplier  = 0;
	cTimeout.ReadTotalTimeoutConstant    = 1000;
	cTimeout.WriteTotalTimeoutMultiplier = 0;
	cTimeout.WriteTotalTimeoutConstant   = 0;

	// ������ ��� ����̽��� �б⾲�� Ÿ�Ӿƿ� ����
	SetCommTimeouts(hFile, &cTimeout);

	nPortOpen = 1;
	return true;
#endif
}

int 
SerialReadByte(void *dummy)
{
#if 0
	DWORD	dwByte;
	char	szRecv[10];
	int		nRet;

	while(nPortOpen){
		// �� ���� ����
		nRet = ReadFile(hFile, szRecv, 1, &dwByte, 0);
		
		// ReadFile()�� �����ϸ� 0�ܸ̿� ��ȯ, Ÿ�Ӿƿ��� ����
		if(dwByte != 1)
			continue;

		SerialWriteByte(szRecv[0]);

		if(ws != 0){
			if(ws->len_rx >=  WS_BUF_LEN ){
                WorkSetFree( ws );
                ws = 0;
                continue;
            }

			ws->rx_buf[ws->len_rx] = szRecv[0];
            ws->len_rx++;

			if(szRecv[0] == LF){
                WorkSetSetState( ws, WS_ACTIVE_IN );
                ws = 0;
            }
		}else{
			if(szRecv[0] != '[')
				continue;

			ws = WorkSetAlloc();
			if(ws == 0){
				printf("error : workset alloc\n");
				continue;
			}
			ws->rx_buf[ws->len_rx] = szRecv[0];
			ws->len_rx++;
		}
	}
#endif

	return true;
}

int timeout = 1;

unsigned char 
SerialReadByte2(void)
{
#ifdef __WIN32__
	DWORD dwByte;
	unsigned char rx = 0;
    DWORD dwBytesTransferred=0;
    time_t startTime;
	startTime = time( NULL ); // Read current time in seconds
	
	do{
		ReadFile(hFile, &rx, 1, &dwByte, 0);
		
		// ReadFile()�� �����ϸ� 0�ܸ̿� ��ȯ, Ÿ�Ӿƿ��� ����
		if(dwByte == 1)
			return rx;

	}while(time(NULL) - startTime < timeout );

	return rx;
#endif
	return 0;
}

int 
SerialReadData(char *buf, int size)
{
#ifdef  __WIN32__
	int i;
	DWORD dwByte;
	int nRet;

	nRet = ReadFile(hFile, buf, size, &dwByte, 0);
		
		// ReadFile()�� �����ϸ� 0�ܸ̿� ��ȯ, Ÿ�Ӿƿ��� ����
	return dwByte;
#endif
	return 0;
}

int 
SerialWriteByte(char byte)
{
#ifdef __WIN32__
	DWORD	nByte;
	char szSend[10];
	szSend[0] = byte;

	WriteFile(hFile, szSend, 1, &nByte, 0);	
	return nByte;
#endif
	return 0;
}

int 
SerialWriteData(char *buf, int size)
{
#ifdef __WIN32__
	DWORD	nByte;
	
	WriteFile(hFile, buf, size, &nByte, 0);	
	return nByte;
#endif
	return 0;
}

int 
SerialClose(void)
{

#ifdef __WIN32__
	CloseHandle(hFile);
	nPortOpen = 0;
#endif

	return  true;
}
