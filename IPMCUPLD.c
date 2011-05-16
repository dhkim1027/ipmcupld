// IPMCUPLD.cpp : Defines the entry point for the console application.
//
#ifdef __WIN32__
#include <Windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "WorkSet.h"
#include "Serial.h"
#include "IpmiHpmFwUpg.h"

void
Help(char *prog)
{
	printf("Usage : %s [COMx] [HPM img file]\n", prog);
}

bool
CheckParams(int argc, char **argv)
{

	//Check COMx port
	if(strncmp(argv[0], "COM", 3)){
		printf("[COMx] : Invalid value %s \n", argv[0]);
		return false;
	}

	return true;
}

int main(int argc, char* argv[])
{
	int i;
	char recv_buf[100];
	if(argc < 3){
		Help(argv[0]);
		return false;
	}

	if(CheckParams(argc-1, &argv[1]) != true)
		return false;

	if( SerialOpen(argv[1], 38400) != true)
		return false;

	HpmfwupgUpgrade(argv[2], 0, 0, VERSIONCHECK_MODE);

	SerialClose();

	return true;
}

