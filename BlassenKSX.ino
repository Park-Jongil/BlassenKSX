#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HardwareSerial.h>
#include "UserDefine.h"
#include "KSX3267.h"
#include "WaterCool.h"


// ESP32-WROOM-32 기준
// GPIO01 TX0 - HardwareSerial (0), 핀번호 변경 불가
// GPIO03 RX0 - HardwareSerial (0), 핀번호 변경 불가
// GPIO10 TX1 - HardwareSerial (1), FLASH 연결에 사용, 핀번호 변경 가능
// GPIO09 RX1 - HardwareSerial (1), FLASH 연결에 사용, 핀번호 변경 가능
// GPIO17 TX2 - HardwareSerial (2), 핀번호 변경 가능
// GPIO16 RX2 - HardwareSerial (2), 핀번호 변경 가능

// HardwareSerial Serial0(0); //3개의 시리얼 중 2번 채널을 사용
// HardwareSerial Serial1(1); //3개의 시리얼 중 2번 채널을 사용
//  HardwareSerial rs485_Serial01(2);  // 장치명 선언(시리얼 번호) < 이걸로 사용해라..

// ESP32-S2-DevKitM-1 에서는 rs485_Serial01(2) 가 Serial1(1) 과 같다.
// HardwareSerial rs485_Serial01(1); ; //2개의 시리얼 중 1번 채널을 사용
// GPIO 6, 7, 8, 9, 10, 11 (the integrated SPI flash)은 플레시 메모리연결에
// 할당되어 있어서 해당핀을 사용할 수 없다

//-------------------------------------------------------------------------------
// H/W Serial
//-------------------------------------------------------------------------------
HardwareSerial rs485_Serial01(1); // 장치명 선언(시리얼 번호) - Main Controller
HardwareSerial rs485_Serial02(2); // 장치명 선언(시리얼 번호) - 대흥금속 냉난방기 모듈

//-------------------------------------------------------------------------------
// LCD 관련함수들
//-------------------------------------------------------------------------------
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
// 통신관련된 버퍼
//-------------------------------------------------------------------------------
unsigned char       szRecvBuf[2][1024];
short int           iRecvCount[2] = { 0 , 0 };
unsigned char       iDeviceID = 0x01;
short int           iSerial_Menu = 0;
short int           iLastOPID = -1;
//-------------------------------------------------------------------------------
// EEPROM
//-------------------------------------------------------------------------------
ProductInfo         stProductInfo;
NetworkInfo         stNetWorkInfo;
//-------------------------------------------------------------------------------
int                 iDeviceCount = 0;
DeviceInfo          stDeviceInfo[MAX_DEVICE_COUNT];
//-------------------------------------------------------------------------------

//-------------------------------------------------------------------------------
// KSX3267 MemoryStack
//---------------------------------------------------------------------------
extern unsigned short int  KSX3267_Memory[1024];
extern NodeHeader          stDeviceHead;
extern NodeHeader          *pDeviceHead;
extern short int           *pDeviceList;
extern ControlStatus       *pControlStatus;
extern ControlCmd          *pContromCmd;
//---------------------------------------------------------------------------
uint8_t blue = 26;

//---------------------------------------------------------------------------
DeviceInfo  *DeviceInfo_Search_byID(int iChkID)
{
  for(int i=0;i<MAX_DEVICE_COUNT;i++) {
    if (stDeviceInfo[i].iEnabled==true && stDeviceInfo[i].iDeviceID==iChkID) return(&stDeviceInfo[i]);
  }
  return(NULL);
}

//---------------------------------------------------------------------------
DeviceInfo  *DeviceInfo_Search_byMapID(int iMapID)
{
  for(int i=0;i<MAX_DEVICE_COUNT;i++) {
    if (stDeviceInfo[i].iEnabled==true && stDeviceInfo[i].iMemoryMapID==iMapID) return(&stDeviceInfo[i]);
  }
  return(NULL);
}

//-------------------------------------------------------------------------------
void setup()
{
    EEPROM_Get_DeviceInformation();
    if (stProductInfo.iSlaveID < 1) stProductInfo.iSlaveID = 0x01;

    Serial.begin(115200); // UART0 -> usb 연결과 공유, 핀번호 변경 불가
                          // Serial1.begin(115200); // 통신에 할당된 기본핀이 플래시 메모리에 할당된 GPIO 10번과 9번이므로 사용불가

    // u1TXD 35 u1RXD 33
    // u2TXD 16 u2RXD 17

    // 이건 안되는데
    // rs485_Serial01.begin(9600, SERIAL_8N1, 33, 35); // (통신속도, UART모드, RX핀번호, TX핀번호) - 핀번호 지정가능
    // rs485_Serial02.begin(9600, SERIAL_8N1, 17, 16); // 핀번호는 기본으로 할당된 번호도 사용할 수 있다.

    //  이건 되네 RX/TX를 변경하면 됨
    // 35 -> 33,  33 -> 35


    rs485_Serial01.begin(9600, SERIAL_8N1, 35, 33); // (통신속도, UART모드, RX핀번호, TX핀번호) - 핀번호 지정가능
    rs485_Serial02.begin(9600, SERIAL_8N1, 17, 16); // 핀번호는 기본으로 할당된 번호도 사용할 수 있다.
    delay(100);

// LCD 설정화면
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
      Serial.println(F("SSD1306 allocation failed"));
      for(;;);
    }
    delay(2000);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    LCD_Display_Message(0,16,"Board Initialize..");

    // initialize digital pin ledPin as an output.
    pinMode(blue, OUTPUT);  

    memset((char*)&KSX3267_Memory, 0x00, sizeof(KSX3267_Memory));
    KSX3267_Memory[1] = 0x00;   // AgencyCode
    KSX3267_Memory[2] = 0x00;   // CompanyCode
    KSX3267_Memory[3] = 0x02;   // ProductType (1:센서노드)(2:구동기노드)
    KSX3267_Memory[4] = 0x00;   // ProductCode
    KSX3267_Memory[5] = 10;     // ProtocolVer (0x10 --> 10으로 변경 : 2022.10.11)
    KSX3267_Memory[6] = 24;     // DeviceCount (기본 : 30)(구동기:24)
    KSX3267_Memory[7] = 0x00;   // DeviceSerial1
    KSX3267_Memory[8] = 0x00;   // DeviceSerial2
//    KSX6267_Memory[101] = 0x01;     // 온도센서
//    KSX6267_Memory[104] = 0x02;     // 습도센서

// 냉난방기 및 제습기 데이터구조체 초기화
    iDeviceCount = 0;
    for(int i=0;i<MAX_DEVICE_COUNT;i++) {
      stDeviceInfo[i].iEnabled = false;
      memset((char*)&stDeviceInfo[i],0x00,sizeof(DeviceInfo));
    }
// rs485_Serial02 를 통한 Slave 장비검색(초기에 한번만 수행 , 장비가 새로 추가되었을 경우에 리셋을 통한 장비리스트 갱신이 필요함)
    SlaveDevice_ScanbyTime();  
  
    Serial_Display_MainDevice_Information();
}

//-------------------------------------------------------------------------------
// EEPROM 에 저장되어 있는 설정값을 획득한다. 
//-------------------------------------------------------------------------------
void  EEPROM_Get_DeviceInformation()
{
  int   iPosition=0x00;

  EEPROM.begin(sizeof(NetworkInfo)+sizeof(ProductInfo));
  EEPROM.get(iPosition,stNetWorkInfo);     // Network 정보를 읽는다.
  iPosition = sizeof(NetworkInfo);
  EEPROM.get(iPosition,stProductInfo);     // 장비의 설정정보를 읽는다.
  iPosition = sizeof(NetworkInfo)+sizeof(ProductInfo);
}

//-------------------------------------------------------------------------------
// EEPROM 에 저장되어 있는 설정값을 획득한다. 
//-------------------------------------------------------------------------------
void  EEPROM_Set_DeviceInformation()
{
  int   iPosition=0x00;

  EEPROM.begin(sizeof(NetworkInfo)+sizeof(ProductInfo));
  EEPROM.put(iPosition,stNetWorkInfo);     // Network 정보를 읽는다.
  iPosition = sizeof(NetworkInfo);
  EEPROM.put(iPosition,stProductInfo);     // 장비의 설정정보를 읽는다.
  iPosition = sizeof(NetworkInfo)+sizeof(ProductInfo);
  EEPROM.commit();
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void LCD_Display_Recv_Message(int Cx,int Cy,int iCmdType,int MsgSize,unsigned char *szMessage)
{
  char szBuffer[1024],szTemp[16];
  
  display.clearDisplay();
  display.setCursor(0, 0);
  sprintf(szBuffer,"KSX3267 Blassen[%2d]",stProductInfo.iSlaveID);
  display.println(szBuffer);
  display.setCursor(Cx, Cy);
  sprintf(szBuffer,"%s [ %d ]",iCmdType==1?"KSX3267":"WaterCooler", MsgSize);
  display.println(szBuffer);
  display.setCursor(0, 28);
  memset(szBuffer,0x00,sizeof(szBuffer));
  for(int i=0;i<MsgSize && i<32;i++) {
    sprintf(szTemp,"%02X ",szMessage[i]);
    strcat(szBuffer,szTemp);
  }
  display.println(szBuffer);
  display.display();
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void LCD_Display_Message(int Cx,int Cy,char *szMessage)
{
  char szBuffer[1024];
  
  display.clearDisplay();
  display.setCursor(0, 0);
  sprintf(szBuffer,"KSX3267 Blassen[%2d]",stProductInfo.iSlaveID);
  display.println(szBuffer);
  display.setCursor(Cx, Cy);
  sprintf(szBuffer,"%-20s", szMessage);
  display.println(szBuffer);
  display.display();
}


//-------------------------------------------------------------------------------
void Serial_Display_DebugMessage(int iMode,char *szChannel,unsigned char *szCommand, int iCount)
{
  char  szBuffer[128];

  sprintf(szBuffer," >> %s [%s] [%2d] : ",(iMode==0x01?"Send":"Recv"),szChannel,iCount);
  Serial.print(szBuffer);
  for(int i=0;i<iCount && i<32;i++) {
    sprintf(szBuffer,"%02X ",szCommand[i]);
    Serial.print(szBuffer);
  }
  Serial.println();
}

//-------------------------------------------------------------------------------
// WaterCooler 에게 보내는 명령(보드기준 오른쪽)
//-------------------------------------------------------------------------------
void  SendMessage_ToDevice(unsigned char *szSendMsg, int iCount)
{
//  digitalWrite(RS485_TX_ENABLE, HIGH);
  rs485_Serial02.write(szSendMsg, iCount);
  rs485_Serial02.flush();
//  digitalWrite(RS485_TX_ENABLE, LOW);
}

//-------------------------------------------------------------------------------
// KSX3267 Master 에게 보내는 명령(보드기준 왼쪽)
//-------------------------------------------------------------------------------
void  SendMessage_ToServer(unsigned char *szSendMsg, int iCount)
{
//  digitalWrite(RS485_TX_ENABLE, HIGH);
  rs485_Serial01.write(szSendMsg, iCount);
  rs485_Serial01.flush();
//  digitalWrite(RS485_TX_ENABLE, LOW);
}

//---------------------------------------------------------------------------
unsigned short int ModRTU_CRC(unsigned char *buf, int len)
{
  unsigned short int crc = 0xFFFF;

  for (int pos = 0; pos < len; pos++) {
    crc ^= (unsigned short int)buf[pos];          // XOR byte into least sig. byte of crc
    for (int i = 8; i != 0; i--) {        // Loop over each bit
      if ((crc & 0x0001) != 0) {      // If the LSB is set
        crc >>= 1;                    // Shift right and XOR 0xA001
        crc ^= 0xA001;
      } else {                            // Else LSB is not set
        crc >>= 1;                    // Just shift right
      }
    }
  }
  // Note, this number has low and high bytes swapped, so use it accordingly (or swap bytes)
  return crc;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
unsigned short int Convert_Endian(unsigned short int iChkData)
{
  unsigned short int   iChkRet;

  iChkRet = (iChkData % 0x100) * 0x100 + (iChkData / 0x100);
  return (iChkRet);
}

//-------------------------------------------------------------------------------
// MainSerial(USB) 을 통하여 메뉴모드로 동작중
//-------------------------------------------------------------------------------
int   iSerialCmd = 0;
char  szSerialCmd[64];
//-------------------------------------------------------------------------------
int  Serial_Menu_Get_String()
{
  unsigned char   nData;

  if (iSerialCmd==0x00) memset(szSerialCmd,0x00,sizeof(szSerialCmd));
  while (Serial.available() > 0) {
    nData = Serial.read();
    szSerialCmd[iSerialCmd] = nData;
    iSerialCmd = iSerialCmd + 1;
  }
  if (nData==0x0A) return(0x01); 
  return(0x00);  
}

//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
void  Serial_Menu_Display(int iMenu)
{
  char    szBuffer[64];
  struct tm timeinfo;  
  time_t  now;
    
  if (iSerial_Menu==0x00) {       // Main Menu
    Serial.println("0.WaterCooler Management Program");
    Serial.println("  [1] Device Information");
    Serial.println("  [2] Check Entire Device");
    Serial.println("  [3] Slave Device Informations");
    Serial.println("  [8] Configuration Save");
    Serial.println("  [9] System Restart..");
  } else if (iSerial_Menu==0x01) {
    Serial.println("1.Device Information");
    Serial.println("  [0] return");
    Serial.println("  [1] Set DeviceID ");
  } else if (iSerial_Menu==0x03) {
    Serial.println("3.Slave Device Informations");
    Serial.println("  [0] return");
    Serial.println("  [1] Display Device ID/Type");
  }
}

//-------------------------------------------------------------------------------
// MainSerial(USB) 을 통하여 메뉴모드로 동작중
//-------------------------------------------------------------------------------
void  Serial_Menu_Command()
{
  int   iRet=0,iChannel,iCount;
  char  szBuffer[128];

  iRet = Serial_Menu_Get_String();
  if (iRet==0x00) return;  
  iSerialCmd = 0;
  Serial.print(" ==> Command = ");       
  for(int i=0;i<strlen(szSerialCmd);i++) {
    sprintf(szBuffer,"%02X ",szSerialCmd[i]);
    Serial.print(szBuffer);       
  }      
  Serial.println();       
  for(int i=0;i<strlen(szSerialCmd);i++) {
    if (szSerialCmd[i]==0x0D) szSerialCmd[i]=0x00;
  }
 
// 엔터까지 기다려서 명령을 확인하는 것으로 한다.   
  if (iSerial_Menu==0x00) {       // Main Menu
    if (strcmp(szSerialCmd,"0")==0x00) {
      Serial_Menu_Display(iSerial_Menu);    
    } else if (strcmp(szSerialCmd,"1")==0x00) {
      iSerial_Menu = 0x01;
      Serial_Display_MainDevice_Information();
      Serial_Menu_Display(iSerial_Menu);    
    } else if (strcmp(szSerialCmd,"2")==0x00) {
      SlaveDevice_ScanbyTime(); 
      Serial_Menu_Display(iSerial_Menu);    
    } else if (strcmp(szSerialCmd,"3")==0x00) {
      Serial_Display_WaterCooler_Information();
      Serial_Menu_Display(iSerial_Menu);    
    } else if (strcmp(szSerialCmd,"8")==0x00) {
      EEPROM_Set_DeviceInformation();
      Serial_Menu_Display(iSerial_Menu);    
    } else if (strcmp(szSerialCmd,"9")==0x00) {
      Serial.print("System Reset.....");  
      ESP.restart();    
    }
  } else if (iSerial_Menu==0x01) {
    if (strcmp(szSerialCmd,"0")==0x00) {
      iSerial_Menu = 0x00;   
      Serial_Menu_Display(iSerial_Menu);    
    } else if (strcmp(szSerialCmd,"1")==0x00) {
      Serial.print("  Device ID = ");  
      iSerial_Menu = 0x11;     
    }
  } else if (iSerial_Menu==0x11) {
    stProductInfo.iSlaveID = atoi(szSerialCmd);  
    EEPROM_Set_DeviceInformation();
    iSerial_Menu = 0x01;
    Serial_Menu_Display(iSerial_Menu);  
  }
}

//-------------------------------------------------------------------------------
// MainSerial(USB) - ESP32 Device Information
//-------------------------------------------------------------------------------
void Serial_Display_MainDevice_Information()
{
  int     i;
  char    szBuffer[1024];

  Serial.println("...Display Main Device Informations...");
  sprintf(szBuffer,"DeviceID(485) = %d",stProductInfo.iSlaveID);
  Serial.println(szBuffer);
  Serial.println("...");
}

//-------------------------------------------------------------------------------
// MainSerial(USB) - WaterCooler Device Information
//-------------------------------------------------------------------------------
void Serial_Display_WaterCooler_Information()
{
  int     i;
  char    szBuffer[1024];

  Serial.println("...Display WaterCooler Device Informations...");
  Serial.println("SEQ\tDeviceID\tKSX_ID\tON/OFF\tSystemMode\tTEMP");
  for(i=0;i<MAX_DEVICE_COUNT;i++) {
    if (stDeviceInfo[i].iEnabled==true) {
      sprintf(szBuffer,"%02d\t%02d\t\t%03d\t%02d\t%03d\t\t%03d",i,stDeviceInfo[i].iDeviceID,stDeviceInfo[i].iMemoryMapID,stDeviceInfo[i].iOperation,stDeviceInfo[i].iSystemMode,stDeviceInfo[i].iCurTemperature);
      Serial.println(szBuffer);
    }
  }
  Serial.println("...");
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

void loop()
{
  char  szBuffer[128];
  unsigned char   nData;
  static int      iTimeCount = 0;
  static unsigned long timepoint = millis();

  if ((millis()-timepoint)>(900U)) {                  //time interval: 0.8s , 1초마다하면 특정초를 넘어갈수있다.  0.99 --> 2.01
    timepoint = millis();
    iTimeCount++;
    if (iTimeCount > 10) {    // 10초에 한번씩 장비의 상태를 
      LCD_Display_Message(0,40,"Get Status Polling..");
      SlaveDevice_GetStatusbyTime();
      iTimeCount = 0; 
    }
    if (iTimeCount == 5) {    // 10초에 한번씩 장비의 상태를 
      LCD_Display_Message(0,40,"");
    }
//    sprintf(szBuffer,"TimeCount = %d\n",iTimeCount);
//    Serial.print(szBuffer);       
  }
// Serial#1 에 데이터가 수신되었는지를 확인하여 버퍼에 저장 (MainController)
  while (rs485_Serial01.available() > 0) {
    nData = rs485_Serial01.read();
    szRecvBuf[0][iRecvCount[0]] = nData;
    iRecvCount[0]++;
  }
// Serial#2 에 데이터가 수신되었는지를 확인하여 버퍼에 저장 (대흥금속 냉난방기 콘트롤러)
  while (rs485_Serial02.available() > 0) {
    nData = rs485_Serial02.read();
    szRecvBuf[1][iRecvCount[1]] = nData;
    iRecvCount[1]++;
  }

// Serial#1 의 버퍼에 내용이 있는지 확인하여 대흥금속 프로토콜 검사
  if (iRecvCount[0] > 0) KSX3267_Receive_Message();  
// Serial#2 의 버퍼에 내용이 있는지 확인하여 대흥금속 프로토콜 검사
  if (iRecvCount[1] > 0) WaterCooller_Receive_Message(0x01);  

// Serial 통신을 이용하여 TCP 데이터 확인
  while (Serial.available() > 0) {
    Serial_Menu_Command();
  }
   
}


/*
 
[0000][Recv] 2022-08-14 10:22:38.54 [   7]
[0000] : 02 01 31 00 00 F3 0D                            ..1....         
[Send] 2022-08-14 10:22:38.54 [   9]
[0000] : 02 01 B1 02 00 01 01 E3 84  

 */
