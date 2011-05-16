#ifdef __WIN32__
#include <Windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __LINUX__
#include <unistd.h>
#endif
#include <time.h>
#include <ctype.h>
#include "WorkSet.h"
#include "IPMI.h"
#include "Serial.h"
#include "Terminal.h"
#include "Md5.h"
#include "IpmiHpmFwUpg.h"

static const int HPMFWUPG_SUCCESS              = 0;
static const int HPMFWUPG_ERROR                = -1;
/* Upload firmware specific error codes */
static const int HPMFWUPG_UPLOAD_BLOCK_LENGTH  = 1;
static const int HPMFWUPG_UPLOAD_RETRY         = 2;

VERSIONINFO gVersionInfo[HPMFWUPG_COMPONENT_ID_MAX];


int 
HpmfwupgGetBufferFromFile(char* imageFilename, struct HpmfwupgUpgradeCtx* pFwupgCtx)
{
   int rc = HPMFWUPG_SUCCESS;
   FILE* pImageFile = fopen(imageFilename, "rb");

   if ( pImageFile == NULL )
   {
      printf("Cannot open image file %s", imageFilename);
      rc = HPMFWUPG_ERROR;
   }
   
   if ( rc == HPMFWUPG_SUCCESS )
   {
      /* Get the raw data in file */
      fseek(pImageFile, 0, SEEK_END);
      pFwupgCtx->imageSize  = ftell(pImageFile);
      pFwupgCtx->pImageData = (unsigned char*)malloc(sizeof(unsigned char)*pFwupgCtx->imageSize);
      rewind(pImageFile);
      if ( pFwupgCtx->pImageData != NULL )
      {
         fread(pFwupgCtx->pImageData, sizeof(unsigned char), pFwupgCtx->imageSize, pImageFile);
      }
      else
      {
         rc = HPMFWUPG_ERROR;
      }

      fclose(pImageFile);
   }

   return rc;

}

unsigned char 
HpmfwupgCalculateChecksum(unsigned char* pData, unsigned int length)
{
   unsigned char checksum = 0;
   unsigned int dataIdx = 0;

   for ( dataIdx = 0; dataIdx < length; dataIdx++ )
   {
      checksum += pData[dataIdx];
   }
   return checksum;
}

int 
HpmfwupgValidateImageIntegrity(struct HpmfwupgUpgradeCtx* pFwupgCtx)
{
   int rc = HPMFWUPG_SUCCESS;
   struct HpmfwupgImageHeader* pImageHeader = (struct HpmfwupgImageHeader*)
                                              pFwupgCtx->pImageData;
   
   md5_state_t ctx;
   static unsigned char md[HPMFWUPG_MD5_SIGNATURE_LENGTH];
   unsigned char* pMd5Sig = pFwupgCtx->pImageData +
                           (pFwupgCtx->imageSize -
                            HPMFWUPG_MD5_SIGNATURE_LENGTH);
   
   /* Validate MD5 checksum */
   memset(md, 0, HPMFWUPG_MD5_SIGNATURE_LENGTH);
   memset(&ctx, 0, sizeof(md5_state_t));
   md5_init(&ctx);
   md5_append(&ctx, pFwupgCtx->pImageData, pFwupgCtx->imageSize -
                                           HPMFWUPG_MD5_SIGNATURE_LENGTH);
   md5_finish(&ctx, md);
   if ( memcmp(md, pMd5Sig,HPMFWUPG_MD5_SIGNATURE_LENGTH) != 0 )
   {
      printf("\n    Invalid MD5 signature\n");
      rc = HPMFWUPG_ERROR;
   }
   
   if ( rc == HPMFWUPG_SUCCESS )
   {
      /* Validate Header signature */
      if( strncmp(pImageHeader->signature, HPMFWUPG_IMAGE_SIGNATURE, HPMFWUPG_HEADER_SIGNATURE_LENGTH) == 0 )
      {
         /* Validate Header image format version */
         if ( pImageHeader->formatVersion == HPMFWUPG_IMAGE_HEADER_VERSION )
         {
            /* Validate header checksum */
            if ( HpmfwupgCalculateChecksum((unsigned char*)pImageHeader,
                                           sizeof(struct HpmfwupgImageHeader) +
                                           pImageHeader->oemDataLength +
                                           sizeof(unsigned char)/*checksum*/) != 0 )
            {
               printf("\n    Invalid header checksum");
               rc = HPMFWUPG_ERROR;
            }
         }
         else
         {
            printf("\n    Unrecognized image version");
            rc = HPMFWUPG_ERROR;
         }
      }
      else
      {
         printf("\n    Invalid image signature");
         rc = HPMFWUPG_ERROR;
      }
   }
   return rc;
}

void HpmDisplayLine(char *s, int n)
{
    while (n--) printf ("%c",*s);
    printf("\n");
}

int HpmDisplayVersionHeader(int mode)
{

   if ( mode & IMAGE_VER)
   {
        HpmDisplayLine("-",41 );
        printf("|ID | Name      |        Versions       |\n");
        printf("|   |           | Active| Backup| File  |\n");
        HpmDisplayLine("-",41 );
   }
   else
   {
        HpmDisplayLine("-",33 );
        printf("|ID | Name      |    Versions   |\n");
        printf("|   |           | Active| Backup|\n");
        HpmDisplayLine("-",33 );
   }
   return 0;
}

int 
HpmDisplayVersion(int mode,VERSIONINFO *pVersion)
{
      char descString[12];
      memset(&descString,0x00,12);
      /*
       * Added this to ensure that even if the description string
       * is more than required it does not give problem in displaying it
       */
      strncpy(descString,pVersion->descString,11);
      /*
       * If the cold reset is required then we can display * on it
       * so that user is aware that he needs to do payload power
       * cycle after upgrade
       */
      printf("|%c%-2d|%-11s|",pVersion->coldResetRequired?'*':' ',pVersion->componentId,descString);

      if (mode & TARGET_VER)
      {
              if (pVersion->targetMajor == 0xFF && pVersion->targetMinor == 0xFF)
                printf(" --.-- |");
              else
                printf("%3d.%02x |",pVersion->targetMajor,pVersion->targetMinor);

              if (mode & ROLLBACK_VER)
              {
                    if (pVersion->rollbackMajor == 0xFF && pVersion->rollbackMinor == 0xFF)
                        printf(" --.-- |");
                    else
                        printf("%3d.%02x |",pVersion->rollbackMajor,pVersion->rollbackMinor);
              }
              else
              {
                    printf(" --.-- |");
              }
      }

      if (mode & IMAGE_VER)
      {
              if (pVersion->imageMajor == 0xFF && pVersion->imageMinor == 0xFF)
                printf(" --.-- |");
              else
                printf("%3d.%02x |",pVersion->imageMajor,pVersion->imageMinor);
      }
	  return 0;
}

int 
HpmfwupgPreUpgradeCheck(struct HpmfwupgUpgradeCtx* pFwupgCtx,
                            int componentToUpload,int option)
{
   int rc = HPMFWUPG_SUCCESS;
   unsigned char* pImagePtr;
   struct HpmfwupgActionRecord* pActionRecord;
   unsigned int actionsSize;
   int flagColdReset = 0;
   struct HpmfwupgImageHeader* pImageHeader = (struct HpmfwupgImageHeader*)
                                                         pFwupgCtx->pImageData;
   /* Put pointer after image header */
   pImagePtr = (unsigned char*)
               (pFwupgCtx->pImageData + sizeof(struct HpmfwupgImageHeader) +
                pImageHeader->oemDataLength + sizeof(unsigned char)/*checksum*/);
   
   /* Deternime actions size */
   actionsSize = pFwupgCtx->imageSize - sizeof(struct HpmfwupgImageHeader);

   if (option & VIEW_MODE)
   {
       HpmDisplayVersionHeader(TARGET_VER|ROLLBACK_VER|IMAGE_VER);
   }
   
   /* Perform actions defined in the image */
   while( ( pImagePtr < (pFwupgCtx->pImageData + pFwupgCtx->imageSize -
            HPMFWUPG_MD5_SIGNATURE_LENGTH)) &&
          ( rc == HPMFWUPG_SUCCESS) )
   {
        /* Get action record */
        pActionRecord = (struct HpmfwupgActionRecord*)pImagePtr;
        
        /* Validate action record checksum */
        if ( HpmfwupgCalculateChecksum((unsigned char*)pActionRecord,
                                     sizeof(struct HpmfwupgActionRecord)) != 0 )
        {
            printf("    Invalid Action record.");
            rc = HPMFWUPG_ERROR;
        }
        
        if ( rc == HPMFWUPG_SUCCESS )
        {
            switch( pActionRecord->actionType )
            {
                case HPMFWUPG_ACTION_UPLOAD_FIRMWARE:
                /* Upload all firmware blocks */
                {
                    struct HpmfwupgFirmwareImage* pFwImage;
                   unsigned char* pData;
                   unsigned int   firmwareLength = 0;
                   unsigned char  mode = 0;
                   unsigned char  componentId = 0x00;
                   unsigned char  componentIdByte = 0x00;
                   VERSIONINFO    *pVersionInfo;
    
                   //struct HpmfwupgGetComponentPropertiesCtx getCompProp;
                   
                   /* Save component ID on which the upload is done */
                   componentIdByte = pActionRecord->components.ComponentBits.byte;
                   while ((componentIdByte>>=1)!=0)
                   {
                        componentId++;
                   }
                   pFwupgCtx->componentId = componentId;
                   pFwImage = (struct HpmfwupgFirmwareImage*)(pImagePtr +
                                  sizeof(struct HpmfwupgActionRecord));
    
                   pData = ((unsigned char*)pFwImage + sizeof(struct HpmfwupgFirmwareImage));
                   
                   /* Get firmware length */
                   firmwareLength  =  pFwImage->length[0];
                   firmwareLength |= (pFwImage->length[1] << 8)  & 0xff00;
                   firmwareLength |= (pFwImage->length[2] << 16) & 0xff0000;
                   firmwareLength |= (pFwImage->length[3] << 24) & 0xff000000;
    
                   pVersionInfo = &gVersionInfo[componentId];
    
                   pVersionInfo->imageMajor  = pFwImage->version[0];
                   pVersionInfo->imageMinor  = pFwImage->version[1];
    
                   mode = TARGET_VER | IMAGE_VER;
                   
                   if (pVersionInfo->coldResetRequired)
                   {
                       flagColdReset = 1;
                   }
                   pVersionInfo->skipUpgrade = 0;
                   
                   if (   (pVersionInfo->imageMajor == pVersionInfo->targetMajor)
                  && (pVersionInfo->imageMinor == pVersionInfo->targetMinor))
                  {
                      if (pVersionInfo->rollbackSupported)
                      {
                          /*If the Image Versions are same as Target Versions then check for the
                           * rollback version*/
                          if (   (pVersionInfo->imageMajor == pVersionInfo->rollbackMajor)
                              && (pVersionInfo->imageMinor == pVersionInfo->rollbackMinor))
                          {
                              /* This indicates that the Rollback version is also same as
                               * Image version -- So now we must skip it */
                               pVersionInfo->skipUpgrade = 1;
                          }
                          mode |= ROLLBACK_VER;
                      }
                      else
                      {
                          pVersionInfo->skipUpgrade = 1;
                      }
                   }
                   if (option & VIEW_MODE)
                   {
                        HpmDisplayVersion(mode,pVersionInfo);
                        printf("\n");
                    }
                   pImagePtr = pData + firmwareLength;
                }
                break;
                default:
                   printf("    Invalid Action type. Cannot continue");
                   rc = HPMFWUPG_ERROR;
                break;
            }
        }
    }
    if (option & VIEW_MODE)
    {
        HpmDisplayLine("-",41);
        if (flagColdReset)
        {
            fflush(stdout);
            printf("(*) Component requires Payload Cold Reset");
        }
    }
    return rc;
}

void 
HpmDisplayUpgradeHeader(int option)
{
    printf("\n");
    HpmDisplayLine("-",79 );
    printf("|ID | Name      |    Versions           |    Upload Progress  | Upload| Image |\n");
    printf("|   |           | Active| Backup| File  |0%%       50%%     100%%| Time  | Size  |\n");
    printf("|---|-----------|-------|-------|-------||----+----+----+----||-------|-------|\n");
}

void HpmDisplayUpgrade( int skip, unsigned int totalSent,
                        unsigned int displayFWLength,time_t timeElapsed)
{
    int percent;
    static int old_percent=1;
    if (skip)
    {
        printf("|       Skip        || --.-- | ----- |\n");
        return;
    }
    fflush(stdout);

    percent = ((float)totalSent/displayFWLength)*100;
//    printf("totalSent:%d, displayFWLength:%d, percent:%d\n",totalSent, displayFWLength, percent);
    if (((percent % 5) == 0))
    {
        if (percent != old_percent)
        {
            if ( percent == 0 ) printf("|");
            else if (percent == 100) printf("|");
                 else printf(".");
            old_percent = percent;
        }
    }

    if (totalSent== displayFWLength)
    {
        /* Display the time taken to complete the upgrade */
        printf("| %02d.%02d | %05x |\n",timeElapsed/60,timeElapsed%60,totalSent);
    }
}


void
HpmFwupgTerminalPktProc(unsigned char *buf, struct ipmi_rs *rsp)
{
	unsigned char len_out = 0;
	unsigned char *ptr = buf;
	int nibble[2];
	int nibble_count;
	unsigned char val;
	int count = 0;
	int buf_len;
	ipmi_terminal_mode_response_t tm_hdr = {0,};
	unsigned char *req_ptr = (unsigned char *)&tm_hdr;

	//printf("%s", buf);

	//printf("%c\n", *ptr);
	
	/* first character must be '[' */
	if( strncmp( ( const char * )ptr++, "[", 1 ) ) {
		printf("first char error: %s\n", buf);
		return;
	}

	/* if not prefixed by SYS, check if this is an ipmi message */
	if( strncmp( ( const char * )ptr, "SYS", 3 ) && strncmp( ( const char * )ptr, "sys", 3 ) )
		goto message_process;

	if( ptr = ( unsigned char * )strchr( ( const char * )ptr, ' ' ) ) {
		ptr++;
	}
	else {
		printf( "[ERR]\n" );
		return;
	}

	message_process:
	nibble_count = 0;
	buf_len = strlen( ( const char * )buf );
	while( ptr < buf + buf_len ) {
		if( ( ( ( *ptr >= 'A' ) && ( *ptr <= 'F' ) ) ||
				( ( *ptr >= 'a' ) && ( *ptr <= 'f' ) ) ||
				( ( *ptr >= '0' ) && ( *ptr <= '9' ) ) )&&
			( nibble_count < 2 ) ) {

			nibble[nibble_count] = *ptr;
			nibble_count++;
			ptr++;

			if( nibble_count == 2 ) {
				switch( nibble[0] ) {
					case 'A': case 'a': val = 10 << 4; break;
					case 'B': case 'b': val = 11 << 4; break;
					case 'C': case 'c': val = 12 << 4; break;
					case 'D': case 'd': val = 13 << 4; break;
					case 'E': case 'e': val = 14 << 4; break;
					case 'F': case 'f': val = 15 << 4; break;
					default: val = ( nibble[0] - 48 ) << 4; break;
				}

				switch( nibble[1] ) {
					case 'A': case 'a': val += 10; break;
					case 'B': case 'b': val += 11; break;
					case 'C': case 'c': val += 12; break;
					case 'D': case 'd': val += 13; break;
					case 'E': case 'e': val += 14; break;
					case 'F': case 'f': val += 15; break;
					default: val += ( nibble[1] - 48 ); break;
				}
				*req_ptr++ = val;
				count++;
				nibble_count = 0;
			}

		} else if ( *ptr == ' ' ) {
			//nibble_count = 0;
			ptr++;
			//terminal_putchar( *ptr++ );
		} else if ( *ptr == ']' ) {
            
			//printf("NETFN : %02x\n", tm_hdr.netfn);
			rsp->ccode = tm_hdr.completion_code;
			rsp->data_len = count-4;
            
			/* save response data for caller */
			if (rsp->ccode == 0 && rsp->data_len > 0) {
				memcpy(rsp->data, tm_hdr.data, rsp->data_len);
				rsp->data[rsp->data_len] = 0;
			}
			return;
		} else {
			printf( "[ERR]\n" );
			return;
		}
	}
}

#define TERM_MAX_SIZ        50
static int  curr_seq = 0;
static char hex_chars[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

unsigned char pkt_out[TERM_MAX_SIZ];
unsigned char pkt_in[TERM_MAX_SIZ];
unsigned char term_input[TERM_MAX_SIZ];
unsigned int term_input_index = 0;

//#define __TERM_DEBUG__
static struct ipmi_rs *
HpmfwupSendRecv(struct ipmi_rq * req)
{
    static struct ipmi_rs rsp;
    ipmi_terminal_mode_response_t *tm_resp = (ipmi_terminal_mode_response_t *)pkt_in;
    ipmi_terminal_mode_request_t *tm_req = (ipmi_terminal_mode_request_t *)pkt_out;
    unsigned int len_out;
    unsigned int i, hi_nibble, lo_nibble;
    unsigned char rx_data;
    unsigned char tmode_start = 0;
	unsigned char recv_buf[100];
	int recv_size = 0;
	char send_buf[100];
	int send_size = 0;
	time_t startTime;
	startTime = time( NULL ); // Read current time in seconds
	rsp.ccode = 0;
   
    memset(tm_req, 0, sizeof(ipmi_terminal_mode_request_t));
    tm_req->responder_lun = 0;
    tm_req->netfn = req->msg.netfn;
    tm_req->bridge = 0;
    tm_req->req_seq = curr_seq++;
    tm_req->command = req->msg.cmd;
    memcpy(tm_req->data, req->msg.data, req->msg.data_len);
    len_out = sizeof(ipmi_terminal_mode_request_t) - TERM_MODE_REQ_MAX_DATA_LEN + req->msg.data_len;
    
	send_buf[send_size++] = '[';

#ifdef __TERM_DEBUG__
    printf("[");
#endif

    for(i=0;i<3; i++ ) {
        hi_nibble = pkt_out[i] >> 4;
        lo_nibble = pkt_out[i] & 0x0f;
        
		/*
        SerialWriteByte(hex_chars[hi_nibble]);
        SerialWriteByte(hex_chars[lo_nibble]);
		*/

		send_buf[send_size++] = hex_chars[hi_nibble];
		send_buf[send_size++] = hex_chars[lo_nibble];

        
 #ifdef __TERM_DEBUG__       
        printf("%c", hex_chars[hi_nibble]);
        printf("%c", hex_chars[lo_nibble]);
 #endif
    }
        
    for( i = 3; i < len_out; i++ ) {
        //SerialWriteByte(' ' );
		send_buf[send_size++] = ' ';
#ifdef __TERM_DEBUG__
        printf(" ");
#endif

        hi_nibble = pkt_out[i] >> 4;
        lo_nibble = pkt_out[i] & 0x0f;
        
		/*
        SerialWriteByte(hex_chars[hi_nibble] );
        SerialWriteByte(hex_chars[lo_nibble] );
		*/

		send_buf[send_size++] = hex_chars[hi_nibble];
		send_buf[send_size++] = hex_chars[lo_nibble];
        
#ifdef __TERM_DEBUG__        
        printf("%c", hex_chars[hi_nibble]);
        printf("%c", hex_chars[lo_nibble]);
#endif
    }
	/*
    SerialWriteByte(']' );
    SerialWriteByte(LF );
	*/

	send_buf[send_size++] = ']';
	send_buf[send_size++] = LF;
    
#ifdef __TERM_DEBUG__
    printf("]\n");
#endif

	SerialWriteData(send_buf, send_size);

#ifdef __WIN32__
    Sleep(10);
#endif

#ifdef __LINUX__
	usleep(1000);
#endif


	do{
		unsigned char rx = SerialReadByte2();
		if(recv_size != 0){
			recv_buf[recv_size++] = rx;

			if(rx == LF)
				break;
		}else{
			if(rx != '[')
				continue;

			recv_buf[recv_size++] = rx;
		}
	}while(time(NULL) - startTime < 10 );
	recv_buf[recv_size] = 0;
	
#if 0
	recv_size = SerialReadData(recv_buf, 30);
	recv_buf[recv_size] = 0;
#endif

	HpmFwupgTerminalPktProc(recv_buf, &rsp);
	if(rsp.ccode != 0){
		printf("[%s] ccode : %x\n", __FUNCTION__, rsp.ccode);
	}
	
    return &rsp;
}


struct ipmi_rs * 
HpmfwupgSendCmd(struct ipmi_rq req, struct HpmfwupgUpgradeCtx* pFwupgCtx )
{
    struct ipmi_rs * rsp;
    unsigned char inaccessTimeout = 0, inaccessTimeoutCounter = 0;
    unsigned char upgradeTimeout  = 0, upgradeTimeoutCounter  = 0;
    unsigned int  timeoutSec1, timeoutSec2;
    unsigned char retry = 0;
    static int errorCount=0;

    /*
    * If we are not in upgrade context, we use default timeout values
    */
    if ( pFwupgCtx != NULL )
    {
        inaccessTimeout = pFwupgCtx->targetCap.inaccessTimeout*5;
        upgradeTimeout  = pFwupgCtx->targetCap.upgradeTimeout*5;
    }
    else
    {
        /* keeping the inaccessTimeout to 60 seconds results in almost 2900 retries
        * So if the target is not available it will be retrying the command for 2900
        * times which is not effecient -So reducing the Timout to 5 seconds which is
        * almost 200 retries if it continuously recieves 0xC3 as completion code.
        */
        inaccessTimeout = HPMFWUPG_DEFAULT_UPGRADE_TIMEOUT;
        upgradeTimeout  = HPMFWUPG_DEFAULT_UPGRADE_TIMEOUT;
    }
    
    timeoutSec1 = time(NULL);
    
    
    do{
        rsp = HpmfwupSendRecv(&req);
        if(rsp == NULL){
        }
       
        /* Handle inaccessibility timeout (rsp = NULL if IOL) */
        if( rsp == NULL || rsp->ccode == 0xff || rsp->ccode == 0xc3 || rsp->ccode == 0xd3 ){
        }
       
        /* Handle node busy timeout */
        else if ( rsp->ccode == 0xc0 )
        {
        }
        else
        {
            if( rsp->ccode == IPMI_CC_OK )
            {
                errorCount = 0 ;
            }
            retry = 0;
        }    
    }while( retry );
    
    return rsp;
}

int 
HpmfwupgInitiateUpgradeAction(struct HpmfwupgInitiateUpgradeActionCtx* pCtx,
                                  struct HpmfwupgUpgradeCtx* pFwupgCtx)
{
    int    rc = HPMFWUPG_SUCCESS;
    
    struct ipmi_rs * rsp;
    struct ipmi_rq   req;

    pCtx->req.picmgId = HPMFWUPG_PICMG_IDENTIFIER;
    
    memset(&req, 0, sizeof(req));
    req.msg.netfn    = IPMI_NETFN_PICMG;
    req.msg.cmd      = HPMFWUPG_INITIATE_UPGRADE_ACTION;
    req.msg.data     = (unsigned char*)&pCtx->req;
    req.msg.data_len = sizeof(struct HpmfwupgInitiateUpgradeActionReq);
    
    rsp = HpmfwupgSendCmd(req, pFwupgCtx);
    if ( rsp )
    {
        /* Long duration command handling */
        if ( rsp->ccode == HPMFWUPG_COMMAND_IN_PROGRESS )
        {
            //rc = HpmfwupgWaitLongDurationCmd(intf, pFwupgCtx);
        }
        else if ( rsp->ccode != 0x00 )
        {
            printf("Error initiating upgrade action, compcode = %x\n",  rsp->ccode);
            rc = HPMFWUPG_ERROR;
        }
    }
    else
    {
        printf("Error initiating upgrade action\n");
        rc = HPMFWUPG_ERROR;
    }
    return rc;
}

int 
HpmfwupgUploadFirmwareBlock(struct HpmfwupgUploadFirmwareBlockCtx* pCtx,
                                struct HpmfwupgUpgradeCtx* pFwupgCtx, int count
                               ,unsigned int *imageOffset, unsigned int *blockLength )
{
   int rc = HPMFWUPG_SUCCESS;
   struct ipmi_rs * rsp;
   struct ipmi_rq   req;
   int i;

retry:
         
   pCtx->req.picmgId = HPMFWUPG_PICMG_IDENTIFIER;

   memset(&req, 0, sizeof(req));
   req.msg.netfn    = IPMI_NETFN_PICMG;
   req.msg.cmd      = HPMFWUPG_UPLOAD_FIRMWARE_BLOCK;
   req.msg.data     = (unsigned char*)&pCtx->req;
  /* 2 is the size of the upload struct - data */
   req.msg.data_len = 2 + count;
   
#if 0
   printf("block number:%d ", pCtx->req.blockNumber);
   for(i=0;i<count;i++){
       printf("%02x ", pCtx->req.data[i]);
   }
   printf("\n");
 #endif
   
//   printf("send data: blk %d \n", pCtx->req.blockNumber);
   rsp = HpmfwupgSendCmd(req, pFwupgCtx);
   if ( rsp )
   {
      if ( rsp->ccode == HPMFWUPG_COMMAND_IN_PROGRESS ||
           rsp->ccode == 0x00 )
      {
          if ( rsp->data_len > 1 )
          {
              if ( rsp->data_len == 9 )
              {
             /* rsp->data[1] - LSB  rsp->data[2]  - rsp->data[3] = MSB */
                  *imageOffset = (rsp->data[4] << 24) + (rsp->data[3] << 16) + (rsp->data[2] << 8) + rsp->data[1];
                  *blockLength = (rsp->data[8] << 24) + (rsp->data[7] << 16) + (rsp->data[6] << 8) + rsp->data[5];
              }
              else
              {
              /*
               * The Spec does not say much for this kind of errors where the
               * firmware returned only offset and length so currently returning it
               * as 0x82 - Internal CheckSum Error
               */
                  printf("Error wrong rsp->datalen %d for Upload Firmware block command\n",rsp->data_len);
                  rsp->ccode = HPMFWUPG_INT_CHECKSUM_ERROR;
              }
          }
      }
      /* Long duration command handling */
      if ( rsp->ccode == HPMFWUPG_COMMAND_IN_PROGRESS )
      {
         //rc = HpmfwupgWaitLongDurationCmd(intf, pFwupgCtx);
      }
      else if ( (rsp->ccode != 0x00) && (rsp->ccode != 0xcc) )
      {
          if( (rsp->ccode == 0x82) || (rsp->ccode == 0x83)){
              printf("HPM: block_number:%d, block:%d\n", pCtx->req.blockNumber, rsp->data[4]);
              goto retry;
              //rc = HPMFWUPG_UPLOAD_RETRY;
          }

          /* 
          if ( HPMFWUPG_IS_RETRYABLE(rsp->ccode) )
          {
              printf("HPM: [PATCH]Retryable error detected");
              rc = HPMFWUPG_UPLOAD_RETRY;
          }
          else if ( rsp->ccode == IPMI_CC_REQ_DATA_INV_LENGTH )
          {
            rc = HPMFWUPG_UPLOAD_BLOCK_LENGTH;
          }
          else
          {
            printf("Error uploading firmware block, compcode = %x\n",  rsp->ccode);
            rc = HPMFWUPG_ERROR;
          }
          */
      }
   }
   else
   {
      printf("Error uploading firmware block\n");
      rc = HPMFWUPG_ERROR;
   }
//   rc = HPMFWUPG_SUCCESS;
   return rc;
}

int 
HpmfwupgFinishFirmwareUpload(struct HpmfwupgFinishFirmwareUploadCtx* pCtx,
                                 struct HpmfwupgUpgradeCtx* pFwupgCtx)
{
    int    rc = HPMFWUPG_SUCCESS;
    struct ipmi_rs * rsp;
    struct ipmi_rq   req;
    
    pCtx->req.picmgId = HPMFWUPG_PICMG_IDENTIFIER;

    memset(&req, 0, sizeof(req));
    req.msg.netfn    = IPMI_NETFN_PICMG;
    req.msg.cmd      = HPMFWUPG_FINISH_FIRMWARE_UPLOAD;
    req.msg.data     = (unsigned char*)&pCtx->req;
    req.msg.data_len = sizeof(struct HpmfwupgFinishFirmwareUploadReq);

    rsp = HpmfwupgSendCmd(req, pFwupgCtx);
    
    if ( rsp )
    {
      /* Long duration command handling */
      if ( rsp->ccode == HPMFWUPG_COMMAND_IN_PROGRESS )
      {
         //rc = HpmfwupgWaitLongDurationCmd(intf, pFwupgCtx);
      }
      else if ( rsp->ccode != IPMI_CC_OK )
      {
         printf("Error finishing firmware upload, compcode = %x\n",  rsp->ccode );
         rc = HPMFWUPG_ERROR;
      }
    }
    else
    {
      printf("Error fininshing firmware upload\n");
      rc = HPMFWUPG_ERROR;
    }

    return rc;
}


int 
HpmfwupgUpgradeStage(struct HpmfwupgUpgradeCtx* pFwupgCtx,
                        int componentToUpload, int option)
{
    struct HpmfwupgImageHeader* pImageHeader = (struct HpmfwupgImageHeader*)
                                                         pFwupgCtx->pImageData;
    struct HpmfwupgComponentBitMask componentToUploadMsk;
    struct HpmfwupgActionRecord* pActionRecord;
    
    int              rc = HPMFWUPG_SUCCESS;
    unsigned char*   pImagePtr;
    unsigned int     actionsSize;
    int              flagColdReset = 0;
    time_t           start,end;

    /* Put pointer after image header */
    pImagePtr = (unsigned char*)
               (pFwupgCtx->pImageData + sizeof(struct HpmfwupgImageHeader) +
                pImageHeader->oemDataLength + sizeof(unsigned char)/*checksum*/);

   /* Deternime actions size */
   actionsSize = pFwupgCtx->imageSize - sizeof(struct HpmfwupgImageHeader);

   if (option & VERSIONCHECK_MODE || option & FORCE_MODE)
   {
       HpmDisplayUpgradeHeader(0);
   }
   
   /* Perform actions defined in the image */
    while( ( pImagePtr < (pFwupgCtx->pImageData + pFwupgCtx->imageSize -
            HPMFWUPG_MD5_SIGNATURE_LENGTH)) &&
          ( rc == HPMFWUPG_SUCCESS) )
    {
        /* Get action record */
        pActionRecord = (struct HpmfwupgActionRecord*)pImagePtr;

        /* Validate action record checksum */
        if ( HpmfwupgCalculateChecksum((unsigned char*)pActionRecord,
                                     sizeof(struct HpmfwupgActionRecord)) != 0 )
        {
            printf("    Invalid Action record.");
            rc = HPMFWUPG_ERROR;
        }
      
        if ( rc == HPMFWUPG_SUCCESS )
        {
            switch( pActionRecord->actionType )
            {
                case HPMFWUPG_ACTION_UPLOAD_FIRMWARE:
                /* Upload all firmware blocks */
                {
                    struct HpmfwupgFirmwareImage* pFwImage;
                    struct HpmfwupgInitiateUpgradeActionCtx initUpgActionCmd;
                    struct HpmfwupgUploadFirmwareBlockCtx   uploadCmd;
                    struct HpmfwupgFinishFirmwareUploadCtx  finishCmd;
//                    struct HpmfwupgGetComponentPropertiesCtx getCompProp;
                    VERSIONINFO    *pVersionInfo;
                    
                    unsigned char* pData, *pDataInitial;
                    unsigned char  count;
                    unsigned int   totalSent = 0;
                    unsigned char  bufLength = 0;
                    unsigned int   firmwareLength = 0;
                    
                    unsigned int   displayFWLength = 0;
                    unsigned char  *pDataTemp;
                    unsigned int   imageOffset = 0x00;
                    unsigned int   blockLength = 0x00;
                    unsigned int   lengthOfBlock = 0x00;
                    unsigned int   numTxPkts = 0;
                    unsigned int   numRxPkts = 0;
                    unsigned char  mode = 0;
                    unsigned char  componentId = 0x00;
                    unsigned char  componentIdByte = 0x00;
                    
                    /* Save component ID on which the upload is done */
                    componentIdByte = pActionRecord->components.ComponentBits.byte;
                    while ((componentIdByte>>=1)!=0)
                    {
                        componentId++;
                    }
                    pFwupgCtx->componentId = componentId;
                    
                    /* Initialize parameters */
                    uploadCmd.req.blockNumber = 0;
                    pFwImage = (struct HpmfwupgFirmwareImage*)(pImagePtr +
                                  sizeof(struct HpmfwupgActionRecord));
                    
                    pDataInitial = ((unsigned char*)pFwImage + sizeof(struct HpmfwupgFirmwareImage));
                    pData = pDataInitial;
                    
                    bufLength = 16;
                    
                    /* Get firmware length */
                    firmwareLength  =  pFwImage->length[0];
                    firmwareLength |= (pFwImage->length[1] << 8)  & 0xff00;
                    firmwareLength |= (pFwImage->length[2] << 16) & 0xff0000;
                    firmwareLength |= (pFwImage->length[3] << 24) & 0xff000000;
                    
                    //printf("firmwareLength : %d\n", firmwareLength);
                    
                    if ( (!(1<<componentToUpload & pActionRecord->components.ComponentBits.byte))
                    && (componentToUpload != DEFAULT_COMPONENT_UPLOAD))
                    {
                        /* We will skip if the user has given some components in command line "component 2" */
                        pImagePtr = pDataInitial + firmwareLength;
                        break;
                    }
                    
                    /* Send initiate command */
                    initUpgActionCmd.req.componentsMask = pActionRecord->components;
                    /* Action is upgrade */
                    initUpgActionCmd.req.upgradeAction  = HPMFWUPG_UPGRADE_ACTION_UPGRADE;
                    rc = HpmfwupgInitiateUpgradeAction(&initUpgActionCmd, pFwupgCtx);
                    if (rc != HPMFWUPG_SUCCESS)
                    {
                        printf("Error HpmfwupgInitiateUpgradeAction\n");
                        break;
                    }
                    
                    pVersionInfo = (VERSIONINFO*) &gVersionInfo[componentId];

                    mode = TARGET_VER | IMAGE_VER;
                    if (pVersionInfo->rollbackSupported)
                    {
                        mode |= ROLLBACK_VER;
                    }
                    if ( pVersionInfo->coldResetRequired)
                    {
                        flagColdReset = TRUE;
                    }
                    if( (option & VERSIONCHECK_MODE) && pVersionInfo->skipUpgrade)
                    {
                    
                        HpmDisplayVersion(mode,pVersionInfo);
                        HpmDisplayUpgrade(1,0,0,0);
                        pImagePtr = pDataInitial + firmwareLength;
                        break;
                    }
                    
                    if ((option & DEBUG_MODE))
                    {
                      printf("\n\n Comp ID : %d  [%-20s]\n",pVersionInfo->componentId,pFwImage->desc);
                    }
                    else
                    {
                        HpmDisplayVersion(mode,pVersionInfo);
                    }
                    
                    /* pDataInitial is the starting pointer of the image data  */
                    /* pDataTemp is one which we will move across */
                    pData = pDataInitial;
                    pDataTemp = pDataInitial;
                    lengthOfBlock = firmwareLength;
                    totalSent = 0x00;
                    displayFWLength= firmwareLength;
                    time(&start);
                
                    while ( (pData < (pDataTemp+lengthOfBlock)) && (rc == HPMFWUPG_SUCCESS) )
                    {
                        if ( (pData+bufLength) <= (pDataTemp+lengthOfBlock) )
                        {
                            count = bufLength;
                        }
                        else
                        {
                            count = (unsigned char)((pDataTemp+lengthOfBlock) - pData);
                        }
                        memcpy(&uploadCmd.req.data, pData, bufLength);
                        
                        imageOffset = 0x00;
                        blockLength = 0x00;
                        numTxPkts++;
                        
                        rc = HpmfwupgUploadFirmwareBlock(&uploadCmd, pFwupgCtx, count,
                                                        &imageOffset,&blockLength);
                        numRxPkts++;
                        
                        if ( rc != HPMFWUPG_SUCCESS)
                        {
                           printf("HPMFWUPG_FAIL\n");
                           if ( rc == HPMFWUPG_UPLOAD_BLOCK_LENGTH )
                           {
                               /* Retry with a smaller buffer length */
                               /*
                               if ( strstr(intf->name,"lan") != NULL )
                               {
                                   bufLength -= (unsigned char)8;
                               }
                               else
                               {
                                   bufLength -= (unsigned char)1;
                               }
                               */
                               rc = HPMFWUPG_SUCCESS;
                            }
                            else if ( rc == HPMFWUPG_UPLOAD_RETRY )
                            {
                                rc = HPMFWUPG_SUCCESS;
                            }
                            else
                            {
                                fflush(stdout);
                                printf("\n Error in Upload FIRMWARE command [rc=%d]\n",rc);
                                printf("\n TotalSent:0x%x ",totalSent);
                                /* Exiting from the function */
                                rc = HPMFWUPG_ERROR;
                                break;
                            }
                        }
                        else{
                            if (blockLength > firmwareLength)
                            {
                                printf("\n Error in Upload FIRMWARE command [rc=%d]\n",rc);
                                printf("\n TotalSent:0x%x Img offset:0x%x  Blk length:0x%x  Fwlen:0x%x\n",
                                            totalSent,imageOffset,blockLength,firmwareLength);
                                rc = HPMFWUPG_ERROR;
                                break;
                            }
                            totalSent += count;
                            if (imageOffset != 0x00)
                            {
                               /* block Length is valid  */
                               lengthOfBlock = blockLength;
                               pDataTemp = pDataInitial + imageOffset;
                               pData = pDataTemp;
                               if ( displayFWLength == firmwareLength)
                               {
                                  /* This is basically used only to make sure that we display uptil 100% */
                                  displayFWLength = blockLength + totalSent;
                               }
                            }
                            else
                            {
                                pData += count;
                            }
                            time(&end);
                            
                            /*
                         *   Just added debug mode in case we need to see exactly how many bytes have
                             * gone through - Its a hidden option used mainly should be used for debugging
                             */
                            if ( option & DEBUG_MODE)
                            {
                                fflush(stdout);
                                printf(" Blk Num : %02x        Bytes : %05x \r",
                                                uploadCmd.req.blockNumber,totalSent);
                                if (imageOffset || blockLength)
                                {
                                   printf("\n\r--> ImgOff : %x BlkLen : %x\n",imageOffset,blockLength);
                                }
                                if (displayFWLength == totalSent)
                                {
                                   printf("\n Time Taken %02d:%02d",(end-start)/60, (end-start)%60);
                                   printf("\n\n");
                                }
                            }
                            else
                            {
                               HpmDisplayUpgrade(0,totalSent,displayFWLength,(end-start));
                            }
                            uploadCmd.req.blockNumber++;
                        }
                    }
                    
                    if (rc == HPMFWUPG_SUCCESS)
                    {
                     /* Send finish component */
                     /* Set image length */
                        finishCmd.req.componentId = componentId;
                     /* We need to send the actual data that is sent
                      * not the comlete firmware image length
                      */
                        finishCmd.req.imageLength[0] = totalSent & 0xFF;
                        finishCmd.req.imageLength[1] = (totalSent >> 8) & 0xFF;
                        finishCmd.req.imageLength[2] = (totalSent >> 16) & 0xFF;
                        finishCmd.req.imageLength[3] = (totalSent >> 24) & 0xFF;
                        //printf("totalSent:%d\n", totalSent);
                        rc = HpmfwupgFinishFirmwareUpload(&finishCmd, pFwupgCtx);
                        pImagePtr = pDataInitial + firmwareLength;
                    }
                }
                break;
                default:
                    printf("    Invalid Action type. Cannot continue");
                    rc = HPMFWUPG_ERROR;
                break;
            }
        }
    }   
    HpmDisplayLine("-",79);
     
    return rc;
}

int 
HpmfwupgActivateFirmware(struct HpmfwupgActivateFirmwareCtx* pCtx,
                             struct HpmfwupgUpgradeCtx* pFwupgCtx)
{
	int    rc = HPMFWUPG_SUCCESS;
	struct ipmi_rs * rsp;
	struct ipmi_rq   req;

	pCtx->req.picmgId = HPMFWUPG_PICMG_IDENTIFIER;

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_PICMG;
	req.msg.cmd      = HPMFWUPG_ACTIVATE_FIRMWARE;
	req.msg.data     = (unsigned char*)&pCtx->req;
	req.msg.data_len = sizeof(struct HpmfwupgActivateFirmwareReq) -
						(!pCtx->req.rollback_override ? 1 : 0);

	rsp = HpmfwupgSendCmd(req, pFwupgCtx);
	if ( rsp )
   {
      /* Long duration command handling */
      if ( rsp->ccode == HPMFWUPG_COMMAND_IN_PROGRESS )
      {
         printf("Waiting firmware activation...");
         fflush(stdout);
#if 0
         rc = HpmfwupgWaitLongDurationCmd(intf, pFwupgCtx);

         if ( rc == HPMFWUPG_SUCCESS )
         {
            lprintf(LOG_NOTICE,"OK");
         }
         else
         {
            lprintf(LOG_NOTICE,"Failed");
         }
#endif
      }
      else if ( rsp->ccode != IPMI_CC_OK )
      {
         printf("Error activating firmware, compcode = %x\n",
                            rsp->ccode);
         rc = HPMFWUPG_ERROR;
      }
   }
   else
   {
      printf("Error activating firmware\n");
      rc = HPMFWUPG_ERROR;
   }

	return rc;
}

static int 
HpmfwupgActivationStage(struct HpmfwupgUpgradeCtx* pFwupgCtx)
{
	int rc = HPMFWUPG_SUCCESS;
	struct HpmfwupgActivateFirmwareCtx activateCmd;
	struct HpmfwupgImageHeader* pImageHeader = (struct HpmfwupgImageHeader*)pFwupgCtx->pImageData;

	/* Print out stuf...*/
   printf("    ");
   fflush(stdout);
   /* Activate new firmware */
   rc = HpmfwupgActivateFirmware(&activateCmd, pFwupgCtx);

   if ( rc == HPMFWUPG_SUCCESS )
   {
      
   }

   /* If activation / self test failed, query rollback status if automatic rollback supported */
   if ( rc == HPMFWUPG_ERROR )
   {
      
   }


                                  
	return rc;
}

int 
HpmGetUserInput(char *str)
{
    char userInput[2];
    printf(str);
    scanf("%s",userInput);
    if (toupper(userInput[0]) == 'Y')
    {
        return 1;
    }
    return 0;
}


int 
HpmfwupgUpgrade(char* imageFilename,
                    int activate, int componentToUpload, int option)
{
   int rc = HPMFWUPG_SUCCESS;
   struct HpmfwupgImageHeader imageHeader;
   struct HpmfwupgUpgradeCtx  fwupgCtx;
          
   /*
    *  GET IMAGE BUFFER FROM FILE
    */

   rc = HpmfwupgGetBufferFromFile(imageFilename, &fwupgCtx);
   /*
    *  VALIDATE IMAGE INTEGRITY
    */

   if ( rc == HPMFWUPG_SUCCESS )
   {
      printf("Validating firmware image integrity...");
      fflush(stdout);
      rc = HpmfwupgValidateImageIntegrity(&fwupgCtx);
      if ( rc == HPMFWUPG_SUCCESS )
      {
         printf("OK\n");
         fflush(stdout);
      }
      else
      {
         free(fwupgCtx.pImageData);
      }
   }

   
   /*
    *  PREPARATION STAGE
    */

	if ( rc == HPMFWUPG_SUCCESS )
	{
		if (HpmGetUserInput("\nServices may be affected during upgrade. Do you wish to continue? y/n "))
		{
			rc = HPMFWUPG_SUCCESS;
		}
		else
		{
			rc = HPMFWUPG_ERROR;
		}
	}


#if 0
   if ( rc == HPMFWUPG_SUCCESS )
   {
      printf("Performing preparation stage...");
      fflush(stdout);
      rc = HpmfwupgPreparationStage(intf, &fwupgCtx, option);
      if ( rc == HPMFWUPG_SUCCESS )
      {
         printf("OK\n");
         fflush(stdout);
      }
      else
      {
         free(fwupgCtx.pImageData);
      }
   }
#endif

    /*
    *  UPGRADE STAGE
    */

   if ( rc == HPMFWUPG_SUCCESS )
   {
      if (option & VIEW_MODE)
      {
        printf("\nComparing Target & Image File version");
      }
      else
      {
        printf("\nPerforming upgrade stage: ");
      }
      if (option & VIEW_MODE)
      {
          rc = HpmfwupgPreUpgradeCheck(&fwupgCtx,componentToUpload,VIEW_MODE);
      }
      else
      {
          rc = HpmfwupgPreUpgradeCheck(&fwupgCtx,componentToUpload,0);
          if (rc == HPMFWUPG_SUCCESS )
          {
              rc = HpmfwupgUpgradeStage(&fwupgCtx,componentToUpload,option);
          }
          else
              printf("Error HpmfwupgPreUpgradeCheck\n");
      }

      if ( rc != HPMFWUPG_SUCCESS )
      {
         printf("Error HpmfwupgUpgradeStage\n");
         free(fwupgCtx.pImageData);
      }
   }

   /*
    *  ACTIVATION STAGE
    */

   if ( rc == HPMFWUPG_SUCCESS )
   {
	  printf("Performing activation stage: ");
      rc = HpmfwupgActivationStage(&fwupgCtx);
      if ( rc != HPMFWUPG_SUCCESS )
      {
         free(fwupgCtx.pImageData);
      }

   }
   return rc;
}




