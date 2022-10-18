#include <stdio.h>
#include "Arduino.h"
#include "UserDefine.h"
#include "KSX3267.h"
#include "WaterCool.h"

//-------------------------------------------------------------------------------
// Main Module Variable
//-------------------------------------------------------------------------------
extern int                 iDeviceCount;
extern DeviceInfo          stDeviceInfo[MAX_DEVICE_COUNT];
//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
// 통신관련된 버퍼
//-------------------------------------------------------------------------------
extern unsigned char       szRecvBuf[2][1024];
extern short int           iRecvCount[2];
extern unsigned char       iDeviceID;

//-------------------------------------------------------------------------------
// KSX3267 MemoryStack
//---------------------------------------------------------------------------
extern unsigned short int  KSX3267_Memory[1024];
extern ControlStatus       *pControlStatus;
extern ControlCmd          *pContromCmd;

//---------------------------------------------------------------------------
// 장비가 초기부팅시에 전체 SlaveDevice에 대해서 ID 조회를 요청한다.  
//---------------------------------------------------------------------------
void SlaveDevice_ScanbyTime()
{
    RequestProtocol     stResponse;
    unsigned short int  iChkCRC;

    for(int i=0;i<10;i++) {
      stResponse.STX = 0x02;
      stResponse.SID = 0x01 + i;
      stResponse.CMD = 0x31;
      stResponse.LEN = 0x00;
      iChkCRC = ModRTU_CRC((unsigned char*)&stResponse,stResponse.LEN+5);
//      Serial_Display_DebugMessage(0x01,"[S02]",(unsigned char*)&iChkCRC,2);

      stResponse.DATA[stResponse.LEN+0] = iChkCRC >> 8;;
      stResponse.DATA[stResponse.LEN+1] = iChkCRC % 0x100;;
      SendMessage_ToDevice((unsigned char*)&stResponse,stResponse.LEN+7);
//      Serial_Display_DebugMessage(0x01,"[S02]",(unsigned char*)&stResponse,stResponse.LEN+7);
      delay(50);
    }
    iDeviceCount = 0;
    for(int i=0;i<MAX_DEVICE_COUNT;i++) stDeviceInfo[i].iEnabled = false;
}

//---------------------------------------------------------------------------
// GetStatus( Code:0x33,0x38 , DataLen : 0 )  
//---------------------------------------------------------------------------
void SlaveDevice_GetStatusbyTime()
{
  unsigned short int  iChkCRC;
  RequestProtocol     stResponse;

  for(int i=0;i<MAX_DEVICE_COUNT;i++) {
    if (stDeviceInfo[i].iEnabled==0x01) {
      stDeviceInfo[i].iCurTemperature  = 0;      // 
      stResponse.STX = 0x02;
      stResponse.SID = stDeviceInfo[i].iDeviceID;
      stResponse.CMD = stDeviceInfo[i].iDeviceType==0x01 ? 0x33:0x38;
      stResponse.LEN = 0x00;
      iChkCRC = ModRTU_CRC((unsigned char*)&stResponse,stResponse.LEN+5);
      stResponse.DATA[stResponse.LEN+0] = iChkCRC / 0x100;;
      stResponse.DATA[stResponse.LEN+1] = iChkCRC % 0x100;;
      SendMessage_ToDevice((unsigned char*)&stResponse,stResponse.LEN+7);
    }
  }
}

//---------------------------------------------------------------------------
// Set SystemMode Response( Code:0x14 , DataLen : 3 ) : 
// [ PowerMode , SystemMode , SetTemp ]
//---------------------------------------------------------------------------
void Protocol_Parser_SetSystemMode(int iDeviceID,int iPowerMode,int iSystemMode,int iSetTemp)
{
  RequestProtocol     stSendMsg;
  unsigned short int  iChkCRC;
  DeviceInfo          *pFindPnt;

  pFindPnt = DeviceInfo_Search_byID(iDeviceID);
  if (pFindPnt != NULL) {
     stSendMsg.STX = 0x02;
    stSendMsg.SID = iDeviceID;
    stSendMsg.CMD = 0x34;
    stSendMsg.LEN = 0x03;
    if (iSystemMode==0x00 && iSetTemp==0x00) {
      stSendMsg.DATA[0] = iPowerMode != 0 ? 1 : 0;       // Power Mode 라서 On이면 무조건 SystemMode 로 설정해야 함.
      stSendMsg.DATA[1] = 0x00;
      stSendMsg.DATA[2] = 0x00;
    } else {
      stSendMsg.DATA[0] = iPowerMode != 0 ? 2 : 0;       // Power Mode 라서 On이면 무조건 SystemMode 로 설정해야 함.
      if (stSendMsg.DATA[0]==0x02) {
        stSendMsg.DATA[1] = iSystemMode;       // 시스템모드
        stSendMsg.DATA[2] = iSetTemp + 40;    // 설정온도
      } else {
        stSendMsg.DATA[1] = 0x00;
        stSendMsg.DATA[2] = 0x00;
      }
    }
    iChkCRC = ModRTU_CRC((unsigned char*)&stSendMsg,stSendMsg.LEN+5);
    stSendMsg.DATA[stSendMsg.LEN+0] = iChkCRC / 0x100;
    stSendMsg.DATA[stSendMsg.LEN+1] = iChkCRC % 0x100;
    SendMessage_ToDevice((unsigned char*)&stSendMsg,stSendMsg.LEN+7);
  }
}


//---------------------------------------------------------------------------
// 장비ID 조회 요청메세지( Code:0x31 , DataLen : 0 ) : 전체장비 리스트를 획득
//---------------------------------------------------------------------------
void Protocol_Parser_GetID(RequestProtocol *pChkPnt)
{
  int         iDeviceID,iMapID;
  DeviceInfo  *pFindPnt;

  iDeviceID = pChkPnt->DATA[0];
  pFindPnt = DeviceInfo_Search_byID(iDeviceID);
  if (pFindPnt==NULL && iDeviceCount < MAX_DEVICE_COUNT) {
    stDeviceInfo[iDeviceCount].iEnabled = true;
    stDeviceInfo[iDeviceCount].iDeviceID = pChkPnt->DATA[0];
    stDeviceInfo[iDeviceCount].iDeviceType = pChkPnt->DATA[1];
    iMapID = MemoryMap_Search_byBlank(); 
    stDeviceInfo[iDeviceCount].iMemoryMapID = iMapID;
    KSX3267_Memory[iMapID] = 102;    // 스위치형 구동기
    iDeviceCount = iDeviceCount + 1;
  }
}

//---------------------------------------------------------------------------
// 동작모드 설정조회 응답메세지( Code:0x33 , DataLen : 4 ) : 냉난방기 (주기적으로 획득)
//---------------------------------------------------------------------------
void Protocol_Parser_GetSystemMode(RequestProtocol *pChkPnt)
{
  int   iMapID;
  DeviceInfo      *pFindPnt;

  pFindPnt = DeviceInfo_Search_byID(pChkPnt->SID);
  if (pFindPnt != NULL) {
    pFindPnt->iPower_Mode = pChkPnt->DATA[0];         // 0x00:OFF , 0x01:ON , 0x02:SystemMode
    pFindPnt->iSystemMode = pChkPnt->DATA[1];         // 시스템 모드 0x01:냉방,0x02:난방,0x03:자동,0x04:에러
    pFindPnt->iOperation  = pChkPnt->DATA[2];         // 해당온도에 따라 0x01:동작중 , 0x00:미작동
    pFindPnt->iCurTemperature = pChkPnt->DATA[3] - 40;
    
    iMapID = pFindPnt->iMemoryMapID - 100;
    pControlStatus[iMapID].Status = pFindPnt->iPower_Mode != 0x00 ? 201 : 0x01;
    pControlStatus[iMapID].RemainTime = pFindPnt->iSystemMode * 1000 + pFindPnt->iCurTemperature;
  }
}

//-------------------------------------------------------------------------------
// TCP/IP 및 RS485를 통해서 들어오는 데이터에 대한 처리
//-------------------------------------------------------------------------------
void WaterCooller_Receive_Message(int iChannel)
{
  int     i,iLength;
  char    szMsgBuf[1024],szTemp[128];
  unsigned short int  iChkCRC;
  RequestProtocol     *pChkPnt;

  while(iRecvCount[iChannel] > 0x04) {
    LCD_Display_Recv_Message(0,16,0x02,iRecvCount[iChannel],szRecvBuf[iChannel]);
    pChkPnt = (RequestProtocol*)&szRecvBuf[iChannel];
    if (pChkPnt->STX==0x02) {
      if ((pChkPnt->LEN+7) <= iRecvCount[iChannel]) {
        iChkCRC = ModRTU_CRC(szRecvBuf[iChannel],pChkPnt->LEN+5);
        if (szRecvBuf[iChannel][pChkPnt->LEN+5]==iChkCRC/0x100 && szRecvBuf[iChannel][pChkPnt->LEN+6]==iChkCRC%0x100) {
//          Display_DebugMessage(0x00,iChannel,szRecvBuf[iChannel],(pChkPnt->LEN+7));
          sprintf(szTemp,"    >> Protocol Msg : ID=%02X , CMD=%02X",pChkPnt->SID,pChkPnt->CMD);
          Serial.println(szTemp);                           // 수신된 메세지가 정상적으로 파싱되었다는것을 출력      
          if (0x11<=pChkPnt->CMD && pChkPnt->CMD<=0x19) { // 0x11~0x19 : App Server Recv
  //          SendMessage_ToDevice((char*)pChkPnt,(pChkPnt->LEN+6));
          } else if (0x91<=pChkPnt->CMD && pChkPnt->CMD<=0x99) { // 0x91~0x99 : App Server Send
//            SendMessage_ToAppServer((char*)pChkPnt,(pChkPnt->LEN+6));
//            if (pChkPnt->CMD==0x93) Protocol_Parser_GetSystemMode(pChkPnt);
//            if (pChkPnt->CMD==0x98) Protocol_Parser_GetDehumidifier(pChkPnt);
          } else if (pChkPnt->CMD==0x21) {                // 0x21 : App 에서의 ID 요청
//            Protocol_Parser_GetEntireID(pChkPnt);
          } else if (pChkPnt->CMD==0x22) {                // 0x22 : App 에서의 Statistics Data 요청
//            Protocol_Parser_GetStatistics(pChkPnt);
          } else if (pChkPnt->CMD==0x23) {                // 0x23 : App 에서의 시스템(통신모듈) 시간 설정
//            Protocol_Parser_SetDateTime(pChkPnt);
          } else if (pChkPnt->CMD==0x2F) {                // 0x2F : App 에서의 시스템 재시작 요청
//            if (pChkPnt->SID==0x2F) resetFunc();         
          } else if (pChkPnt->CMD==0xB1) {                // 0x31 : Get Slave ID , 장비의 주기적인 스캔메세지에 대한 처리를 이곳에서 해야한다.     
            Protocol_Parser_GetID(pChkPnt);
          } else if (pChkPnt->CMD==0xB2) {                // 0x32 : Set Slave ID
          } else if (pChkPnt->CMD==0xB3) {                // 0x33 : Get System Mode (주기적으로 스캔하여 데이터 저장 : 통계용)
            Protocol_Parser_GetSystemMode(pChkPnt);
          } else if (pChkPnt->CMD==0xB4) {                // 0x34 : Set System Mode
          } else if (pChkPnt->CMD==0xB5) {                // 0x35 : 시간/온도 설정확인
          } else if (pChkPnt->CMD==0xB6) {                // 0x36 : 시간/온도 설정변경
          } else if (pChkPnt->CMD==0xB7) {                // 0x37 : 시스템 시간변경
          } else if (pChkPnt->CMD==0xB8) {                // 0x38 : 제습기 설정조회 (주기적으로 스캔하여 데이터 저장 : 통계용)
          } else if (pChkPnt->CMD==0xB9) {                // 0x39 : 제습기 설정변경
          }
        }
        iRecvCount[iChannel] = iRecvCount[iChannel] - (pChkPnt->LEN+7);
        memcpy(szRecvBuf[iChannel],(unsigned char*)&szRecvBuf[iChannel][(pChkPnt->LEN+7)],iRecvCount[iChannel]);
      } else {
        return;
      }
    } else {
      iRecvCount[iChannel] = iRecvCount[iChannel] - 1;
      memcpy(szRecvBuf[iChannel],(unsigned char*)&szRecvBuf[iChannel][1],iRecvCount[iChannel]);
    }
  }
}
