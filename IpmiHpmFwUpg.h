#ifndef __IPMI_HPMFWUPG_H__
#define __IPMI_HPMFWUPG_H__

#ifndef TRUE
#define TRUE    1
#endif

#ifndef FALSE
#define FALSE   0
#endif

/*
 *  HPM.1 IMAGE DEFINITIONS
 */

#define HPMFWUPG_HEADER_SIGNATURE_LENGTH 8
#define HPMFWUPG_MANUFATURER_ID_LENGTH   3
#define HPMFWUPG_PRODUCT_ID_LENGTH       2
#define HPMFWUPG_TIME_LENGTH             4
#define HPMFWUPG_TIMEOUT_LENGTH          1
#define HPMFWUPG_COMP_REVISION_LENGTH    2
#define HPMFWUPG_FIRM_REVISION_LENGTH    6
#define HPMFWUPG_IMAGE_HEADER_VERSION    0
#define HPMFWUPG_IMAGE_SIGNATURE "PICMGFWU"

#define HPMFWUPG_DESCRIPTION_LENGTH   21
#define HPMFWUPG_FIRMWARE_SIZE_LENGTH 4


#define TARGET_VER                              (0x01)
#define ROLLBACK_VER                            (0x02)
#define IMAGE_VER                               (0x04)

/*
 *  Options added for user to check the version and to view both the FILE and TARGET Version
 */
#define VERSIONCHECK_MODE                       0x01
#define VIEW_MODE                               0x02
#define DEBUG_MODE                              0x04
#define FORCE_MODE_ALL                          0x08
#define FORCE_MODE_COMPONENT                    0x10
#define FORCE_MODE                              (FORCE_MODE_ALL|FORCE_MODE_COMPONENT)

#define HPMFWUPG_PICMG_IDENTIFIER         0
#define HPMFWUPG_VERSION_SIZE             6
#define HPMFWUPG_DESC_STRING_LENGTH       12
#define HPMFWUPG_DEFAULT_INACCESS_TIMEOUT 60 /* sec */
#define HPMFWUPG_DEFAULT_UPGRADE_TIMEOUT  60 /* sec */
#define HPMFWUPG_MD5_SIGNATURE_LENGTH     16

/*
 *  HPM.1 SPECIFIC COMPLETION CODES
 */
#define HPMFWUPG_ROLLBACK_COMPLETED   0x00
#define HPMFWUPG_COMMAND_IN_PROGRESS  0x80
#define HPMFWUPG_NOT_SUPPORTED        0x81
#define HPMFWUPG_SIZE_MISMATCH        0x81
#define HPMFWUPG_ROLLBACK_FAILURE     0x81
#define HPMFWUPG_INV_COMP_MASK        0x81
#define HPMFWUPG__ABORT_FAILURE       0x81
#define HPMFWUPG_INV_COMP_ID          0x82
#define HPMFWUPG_INT_CHECKSUM_ERROR   0x82
#define HPMFWUPG_INV_UPLOAD_MODE      0x82
#define HPMFWUPG_ROLLBACK_OVERRIDE    0x82
#define HPMFWUPG_INV_COMP_PROP        0x83
#define HPMFWUPG_FW_MISMATCH          0x83
#define HPMFWUPG_ROLLBACK_DENIED      0x83



typedef enum eHpmfwupgComponentId
{
   HPMFWUPG_COMPONENT_ID_0 = 0,
   HPMFWUPG_COMPONENT_ID_1,
   HPMFWUPG_COMPONENT_ID_2,
   HPMFWUPG_COMPONENT_ID_3,
   HPMFWUPG_COMPONENT_ID_4,
   HPMFWUPG_COMPONENT_ID_5,
   HPMFWUPG_COMPONENT_ID_6,
   HPMFWUPG_COMPONENT_ID_7,
   HPMFWUPG_COMPONENT_ID_MAX
} tHpmfwupgComponentId;


typedef struct _VERSIONINFO
{
    int componentId;
    int targetMajor;
    int targetMinor;
    int rollbackMajor;
    int rollbackMinor;
    int imageMajor;
    int imageMinor;
    int coldResetRequired;
    int rollbackSupported;
    int skipUpgrade;
    char descString[15];
}VERSIONINFO, *PVERSIONINFO;


typedef enum eHpmfwupgActionType
{
   HPMFWUPG_ACTION_BACKUP_COMPONENTS = 0,
   HPMFWUPG_ACTION_PREPARE_COMPONENTS,
   HPMFWUPG_ACTION_UPLOAD_FIRMWARE,
   HPMFWUPG_ACTION_RESERVED = 0xFF
} tHpmfwupgActionType;

struct HpmfwupgComponentBitMask
{
   union
   {
      unsigned char byte;
      struct
      {
         unsigned char component0 : 1;
         unsigned char component1 : 1;
         unsigned char component2 : 1;
         unsigned char component3 : 1;
         unsigned char component4 : 1;
         unsigned char component5 : 1;
         unsigned char component6 : 1;
         unsigned char component7 : 1;
      }bitField;
   }ComponentBits;
} ;


struct HpmfwupgImageHeader
{
  char           signature[HPMFWUPG_HEADER_SIGNATURE_LENGTH];
  unsigned char  formatVersion;
  unsigned char  deviceId;
  unsigned char  manId[HPMFWUPG_MANUFATURER_ID_LENGTH];
  unsigned char  prodId[HPMFWUPG_PRODUCT_ID_LENGTH];
  unsigned char  time[HPMFWUPG_TIME_LENGTH];
  union
  {
     struct
     {
        
        unsigned char reserved        : 4;
        unsigned char servAffected    : 1;
        unsigned char manRollback     : 1;
        unsigned char autRollback     : 1;
        unsigned char imageSelfTest   : 1;
      
     }  bitField;
     unsigned char byte;
  }imageCapabilities;
  struct HpmfwupgComponentBitMask components;
  unsigned char  selfTestTimeout;
  unsigned char  rollbackTimeout;
  unsigned char  inaccessTimeout;
  unsigned char  compRevision[HPMFWUPG_COMP_REVISION_LENGTH];
  unsigned char  firmRevision[HPMFWUPG_FIRM_REVISION_LENGTH];
  unsigned short oemDataLength;
} ;

struct HpmfwupgActionRecord
{
   unsigned char  actionType;
   struct HpmfwupgComponentBitMask components;
   unsigned char  checksum;
} ;



struct HpmfwupgFirmwareImage
{
   unsigned char version[HPMFWUPG_FIRM_REVISION_LENGTH];
   char          desc[HPMFWUPG_DESCRIPTION_LENGTH];
   unsigned char length[HPMFWUPG_FIRMWARE_SIZE_LENGTH];
} ;

/*
 * UPGRADE ACTIONS DEFINITIONS
 */

enum 
{
   HPMFWUPG_UPGRADE_ACTION_BACKUP = 0,
   HPMFWUPG_UPGRADE_ACTION_PREPARE,
   HPMFWUPG_UPGRADE_ACTION_UPGRADE,
   HPMFWUPG_UPGRADE_ACTION_COMPARE,
   HPMFWUPG_UPGRADE_ACTION_INVALID = 0xff
};


struct HpmfwupgInitiateUpgradeActionReq
{
   unsigned char picmgId;
   struct HpmfwupgComponentBitMask componentsMask;
   unsigned char upgradeAction;
};

struct HpmfwupgInitiateUpgradeActionResp
{
   unsigned char picmgId;
} ;

struct HpmfwupgInitiateUpgradeActionCtx
{
   struct HpmfwupgInitiateUpgradeActionReq  req;
   struct HpmfwupgInitiateUpgradeActionResp resp;
} ;

#define HPMFWUPG_SEND_DATA_COUNT_MAX   32
#define HPMFWUPG_SEND_DATA_COUNT_KCS   30
#define HPMFWUPG_SEND_DATA_COUNT_LAN   25
#define HPMFWUPG_SEND_DATA_COUNT_IPMB  26
#define HPMFWUPG_SEND_DATA_COUNT_IPMBL 26
#define HPMFWUPG_IMAGE_SIZE_BYTE_COUNT 4

#define DEFAULT_COMPONENT_UPLOAD                0x0F


struct HpmfwupgUploadFirmwareBlockReq
{
   unsigned char picmgId;
   unsigned char blockNumber;
   unsigned char data[HPMFWUPG_SEND_DATA_COUNT_MAX];
} ;

struct HpmfwupgUploadFirmwareBlockResp
{
  unsigned char picmgId;
} ;

struct HpmfwupgUploadFirmwareBlockCtx
{
   struct HpmfwupgUploadFirmwareBlockReq  req;
   struct HpmfwupgUploadFirmwareBlockResp resp;
} ;

struct HpmfwupgFinishFirmwareUploadReq
{
   unsigned char picmgId;
   unsigned char componentId;
   unsigned char imageLength[HPMFWUPG_IMAGE_SIZE_BYTE_COUNT];
} ;

struct HpmfwupgFinishFirmwareUploadResp
{
   unsigned char picmgId;
} ;

struct HpmfwupgFinishFirmwareUploadCtx
{
   struct HpmfwupgFinishFirmwareUploadReq  req;
   struct HpmfwupgFinishFirmwareUploadResp resp;
} ;

struct HpmfwupgGetTargetUpgCapabilitiesResp
{
   unsigned char picmgId;
   unsigned char hpmVersion;
   union
   {
      unsigned char byte;
      struct
      {
         unsigned char ipmcSelftestCap     : 1;
         unsigned char autRollback         : 1;
         unsigned char manualRollback      : 1;
         unsigned char servAffectDuringUpg : 1;
         unsigned char deferActivation     : 1;
         unsigned char ipmcDegradedDurinUpg: 1;
         unsigned char autRollbackOverride : 1;
         unsigned char fwUpgUndesirable    : 1;
         
      }bitField;
   }GlobalCapabilities;
   unsigned char upgradeTimeout;
   unsigned char selftestTimeout;
   unsigned char rollbackTimeout;
   unsigned char inaccessTimeout;
   struct HpmfwupgComponentBitMask componentsPresent;
} ;


struct HpmfwupgUpgradeCtx
{
   unsigned int   imageSize;
   unsigned char* pImageData;
   unsigned char  componentId;
   struct HpmfwupgGetTargetUpgCapabilitiesResp targetCap;
//   struct HpmfwupgGetGeneralPropResp           genCompProp[HPMFWUPG_COMPONENT_ID_MAX];
//   struct ipm_devid_rsp                        devId;
} ;

struct HpmfwupgActivateFirmwareReq
{
   unsigned char picmgId;
   unsigned char rollback_override;
};

struct HpmfwupgActivateFirmwareResp
{
   unsigned char picmgId;
};

struct HpmfwupgActivateFirmwareCtx
{
   struct HpmfwupgActivateFirmwareReq  req;
   struct HpmfwupgActivateFirmwareResp resp;
};



int HpmfwupgUpgrade(char* imageFilename, int activate, int componentToUpload, int option);


#endif
