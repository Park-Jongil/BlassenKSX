#ifndef User_DataStruct_Define
#define User_DataStruct_Define
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
#define   ERROR_NO_FUNCTION      0x01
#define   ERROR_DATA_ADDR        0x02
#define   ERROR_DATA_VALUE       0x03
#define   ERROR_DEV_FAIL         0x04
//---------------------------------------------------------------------------
#define   MAX_DEVICE_COUNT    16
//---------------------------------------------------------------------------
typedef struct  _ProductInfo {
  short int   iSlaveID;           // RS485용 구분을 위한 장치 ID
  char        szCompanyName[32];
  char        szProductName[32];
  char        szProductCode[32];
  char        szSerialNumber[32];     // 제품시리얼번호
  char        szRouterNumber[16];     // LTE Router 번호 (010-XXXX-XXXX)  
  char        szMQTT_Server[64];      // XXX.XXX.XXX.XXX or URL
  short int   iMQTT_Port;
} ProductInfo;

typedef struct  _NetworkInfo {
  char        NetworkEnable;
  char        szIPAddress[32];
  char        szSubnetMask[32];
  char        szGateWay[32];
  char        szDNS_Addr[32];
  short int   iServerPort;
  char        WIFI_SSID[32];             // Wifi SSID
  char        WIFI_Password[32];         // Wifi Password
} NetworkInfo;

//---------------------------------------------------------------------------
typedef struct  _RequestPacket {
  unsigned char   SlaveAddr;      // Slave Address
  unsigned char   Cmd;            // Command Code
  unsigned char   Data[256];      //
} RequestPacket;

typedef struct  _RequestGetData {
  short int   StartAddr;
  short int   iCount;
} RequestGetData;

typedef struct  _RequestWriteData {
  short int   Addr;
  short int   Data;
} RequestWriteData;

typedef struct  _RequestWriteBlock {
  unsigned char   SlaveAddr;      // Slave Address
  unsigned char   Cmd;            // Command Code (0x10)
  short int       Addr;
  short int       Count;
  unsigned char   Length;
  short int       Data[100];
} __attribute__ ((packed)) RequestWriteBlock;

//---------------------------------------------------------------------------
typedef struct  _NodeHeader {
  short int   AgencyCode;
  short int   CompanyCode;
  short int   ProductType;
  short int   ProductCode;
  short int   ProtocolVer;
  short int   DeviceCount;
  short int   DeviceSerial1;
  short int   DeviceSerial2;
} NodeHeader;

typedef struct  _SensorNode {
  union {
    int     intValue;
    float   fValue;
  };
  short int Status;
} SensorNode;

typedef struct  _ControlStatus {
  short int OPID;
  short int Status;
  int   RemainTime;       // ��,��,�� �ѹ���Ʈ�� ���
} ControlStatus;

typedef struct  _ControlCmd {
  short int   CmdOP;
  short int   OPID;
  int         SetTime;
//  short int   Reserved;
} ControlCmd;

//---------------------------------------------------------------------------
// Device Info Structure
//---------------------------------------------------------------------------
typedef struct  _DeviceInfo {
  unsigned char   iEnabled;
  unsigned int    iMemoryMapID;
  unsigned char   iDeviceID;        // 1~10:냉낭방기
  unsigned char   iDeviceType;      // 0x01:냉난방기 , 0x02:제습기
  unsigned char   iSystemMode;
  unsigned char   iPower_Mode;      // 0x00:OFF , 0x01:ON , 0x02:SystemMode
  unsigned char   iOperation;  
  unsigned char   iCurTemperature;
  unsigned char   iSetTemperature;
  int             iRetryCount;
} DeviceInfo;

//-------------------------------------------------------------------------------
// 대흥금속 냉난방기 프로토콜 적용
//-------------------------------------------------------------------------------
struct  _RequestProtocol {
  unsigned char   STX;            // STX : 0x02
  unsigned char   SID;            // SlaveID : 0x01 ~ 0xF0
  unsigned char   CMD;            // Command Code
  short int       LEN;            // Data Length
  unsigned char   DATA[800];      // DataBlock , ETX(0x03)
} __attribute__ ((packed)); 
typedef _RequestProtocol RequestProtocol;

//---------------------------------------------------------------------------
// Public Functions
//---------------------------------------------------------------------------
unsigned short int ModRTU_CRC(unsigned char *buf, int len);
unsigned short int Convert_Endian(unsigned short int iChkData);
unsigned int Convert_Endian_Integer(unsigned int iChkData);
void  SendMessage_ToDevice(unsigned char *szSendMsg, int iCount);
void  SendMessage_ToServer(unsigned char *szSendMsg, int iCount);
DeviceInfo  *DeviceInfo_Search_byID(int iChkID);
DeviceInfo  *DeviceInfo_Search_byMapID(int iMapID);

void LCD_Display_Message(int Cx,int Cy,char *szMessage);
void LCD_Display_Recv_Message(int Cx,int Cy,int iCmdType,int MsgSize,unsigned char *szMessage);
void Serial_Display_DebugMessage(int iMode,char *szChannel,unsigned char *szCommand, int iCount);

//---------------------------------------------------------------------------
#endif
