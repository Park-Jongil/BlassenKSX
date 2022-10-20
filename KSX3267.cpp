#include <stdio.h>
#include "Arduino.h"
#include "UserDefine.h"
#include "KSX3267.h"
#include "WaterCool.h"

//-------------------------------------------------------------------------------
// Main Module Variable
//---------------------------------------------------------------------------
extern unsigned char       szRecvBuf[2][1024];
extern short int           iRecvCount[2];
extern unsigned char       iDeviceID;
extern short int           iLastOPID;
extern DeviceInfo          stDeviceInfo[MAX_DEVICE_COUNT];
//-------------------------------------------------------------------------------
// KSX3267 MemoryStack
//---------------------------------------------------------------------------
unsigned short int  KSX3267_Memory[1024];
NodeHeader          stDeviceHead;
NodeHeader          *pDeviceHead = (NodeHeader*)&KSX3267_Memory[1];
short int           *pDeviceList = (short int*)&KSX3267_Memory[100];
ControlStatus       *pControlStatus = (ControlStatus*)&KSX3267_Memory[199];
ControlCmd          *pContromCmd = (ControlCmd*)&KSX3267_Memory[499];

//---------------------------------------------------------------------------
int MemoryMap_Search_byBlank()
{
  int   i;
  for(i=101;i<200;i++) {
    if (KSX3267_Memory[i]==0) return(i); 
  }
}

//---------------------------------------------------------------------------
void  Protocol_Request_ErrorCode(int SlaveID,int CmdCode,int iErrorCode)
{
  unsigned char  szBuffer[1024],szTemp[1024];
  unsigned short int  iChkCRC;

  szBuffer[0] = SlaveID;
  szBuffer[1] = CmdCode + 0x80;     // 0x03에 대한 예외 응답의 기능코드
  szBuffer[2] = iErrorCode;         // 유효하지 않은 주소를 접근
  iChkCRC = ModRTU_CRC(szBuffer,3);
  szBuffer[3] = iChkCRC % 0x100;
  szBuffer[4] = iChkCRC / 0x100;
  SendMessage_ToServer(szBuffer,5);
}


//-------------------------------------------------------------------------------
void Protocol_Request_DataBlock(int SlaveID, int StartAddr, int iCount)
{
  unsigned char Temp;
  unsigned char  szBuffer[128], szTemp[128];
  unsigned short int  iChkCRC;

  memset(szBuffer, 0x00, sizeof(szBuffer));
  memset(szTemp, 0x00, sizeof(szTemp));
  szBuffer[0] = SlaveID;
  szBuffer[1] = 0x03;

  szBuffer[2] = iCount * 2;
  memcpy((char*)&szBuffer[3], (char*)&KSX3267_Memory[StartAddr], iCount * 2);
  for (int i = 1; i <= iCount; i++) {
    Temp = szBuffer[2 * i + 1];
    szBuffer[2 * i + 1] = szBuffer[2 * i + 2];
    szBuffer[2 * i + 2] = Temp;
  }
  iChkCRC = ModRTU_CRC(szBuffer, 3 + iCount * 2);
  szBuffer[3 + iCount * 2] = iChkCRC % 0x100;
  szBuffer[4 + iCount * 2] = iChkCRC / 0x100;
  SendMessage_ToServer(szBuffer, 5 + iCount * 2);
}

//---------------------------------------------------------------------------
void Protocol_Write_DataBlock(int SlaveID, int Addr, int Data)
{
  unsigned char Temp;
  unsigned char  szBuffer[1024],szTemp[1024];
  unsigned short int  iChkCRC;

  if (iDeviceID!=SlaveID) {
    Protocol_Request_ErrorCode(SlaveID,0x06,0x03);
    return;
  }
  KSX3267_Memory[Addr] = Data;
  if (Addr < 100) {
    memcpy((char*)&stDeviceHead,(char*)&KSX3267_Memory[1],sizeof(stDeviceHead));
  }
// Controller 에 제어의 명령이 발생했는지를 확인할 필요가 있다
  if (500 <= Addr && Addr < 600) {
    
  }

// Reponse Data
  memset(szBuffer,0x00,sizeof(szBuffer));
  szBuffer[0] = SlaveID;
  szBuffer[1] = 0x06;
  memcpy((char*)&szBuffer[2],(char*)&Addr,sizeof(short int));
  memcpy((char*)&szBuffer[4],(char*)&Data,sizeof(short int));
  for(int i=1;i<=2;i++) {
    Temp = szBuffer[2*i];
    szBuffer[2*i] = szBuffer[2*i+1];
    szBuffer[2*i+1] = Temp;
  }
  iChkCRC = ModRTU_CRC(szBuffer,6);
  szBuffer[6] = iChkCRC % 0x100;
  szBuffer[7] = iChkCRC / 0x100;
  SendMessage_ToServer(szBuffer,8);
}

//---------------------------------------------------------------------------
void Protocol_MultiWrite_DataBlock(RequestWriteBlock *pChkPnt)
{
  char            szMessage[1024];
  unsigned char   Temp;
  unsigned char   szBuffer[1024],szTemp[1024];
  unsigned short int  iChkCRC,iAddr,iData,iCount,iMapID,iMapPos;
  DeviceInfo      *pFindPnt;
  ControlCmd      *pChkCtrl,*pCmdOP;
//  ControlCmd      CheckCmd;

  if (iDeviceID!=pChkPnt->SlaveAddr) {
    Protocol_Request_ErrorCode(pChkPnt->SlaveAddr,0x10,0x03);
    return;
  }
// 전송되어진 레지스터값을 메모리에 적재하는것이 우선
  iAddr  = Convert_Endian(pChkPnt->Addr);
  iCount = Convert_Endian(pChkPnt->Count);
  for(int i=0;i<iCount;i++) {
    iData = Convert_Endian(pChkPnt->Data[i]);
    KSX3267_Memory[iAddr+i] = iData;
//    sprintf(szMessage,"    << KSX3267  Addr=%d , Data=%d",iAddr+i,iData);
//    Serial.println(szMessage);
  }
  if (iAddr < 100) {
    memcpy((char*)&stDeviceHead,(char*)&KSX3267_Memory[1],sizeof(stDeviceHead));
  }

// 실제 레지스터에 적용된 다음에 해당 블럭이 데이터블럭인지를 확인해야 한다. (2022.10.20)
// 혹시나 시작주소가 503 이 아닌 501 로 들어온다면 아래의 코드는 정상적으로 동작하지 않는다. 
/*
  pChkCtrl = (ControlCmd*)&KSX3267_Memory[iAddr]; 
  pCmdOP = (ControlCmd*)&pChkPnt->Data;
//  기존의 OPID 값과 신규명령의 OPID 값이 달라야한다. 
//  동일하다면 중복명령 수신으로 판단되어 처리하지 않는다.
  if (pChkCtrl->OPID == Convert_Endian(pCmdOP->OPID)) {
    Protocol_Request_ErrorCode(pChkPnt->SlaveAddr,0x10,0x03);
    return;
  }
*/
  
// Controller 에 제어의 명령이 발생했는지를 확인할 필요가 있다
// 전체장비에 대해서 장비의 제어명령이 속하는 주소의 값이 전달된 데이터블럭에 있는지를 확인해야 한다.(2022.10.20)
// MapID=101 --> 제어명령블럭 503
// MapID=102 --> 제어명령블럭 507
  if (500 <= iAddr && iAddr < 600) {
    for(int i=0;i<MAX_DEVICE_COUNT;i++) {
      if (stDeviceInfo[i].iEnabled==true) { // && stDeviceInfo[i].iMemoryMapID==iMapID) {
        iMapPos = 499 + (stDeviceInfo[i].iMemoryMapID-100)*4;          
        if (iAddr <= iMapPos && iMapPos <= (iAddr+iCount)) {     // 해당전송된 블럭이 해당제어명령을 포함한다면
          pChkCtrl = (ControlCmd*)&KSX3267_Memory[iMapPos]; 
          sprintf(szMessage,"    << KSX3267  CmdOP=%d , OPID=%d , SetTime=%d",pChkCtrl->CmdOP,pChkCtrl->OPID,pChkCtrl->SetTime);
          Serial.println(szMessage);
// 해당하는 iMapID 의 장비에게 센서데이터의 값을 이용하여 On/Off 와 SystemMode , 설정온도를 전송한다.
          if (iLastOPID != pChkCtrl->OPID) {
            Protocol_Parser_SetSystemMode(stDeviceInfo[i].iDeviceID,pChkCtrl->CmdOP,pChkCtrl->SetTime/1000,(pChkCtrl->SetTime%100));
            pControlStatus[iMapID].OPID = pChkCtrl->OPID;
          }
          iLastOPID = pChkCtrl->OPID;
        }
      }
    }
  }
  
// Reponse Data
  memset(szBuffer,0x00,sizeof(szBuffer));
  szBuffer[0] = pChkPnt->SlaveAddr;
  szBuffer[1] = 0x10;
  memcpy((char*)&szBuffer[2],(char*)&pChkPnt->Addr,sizeof(short int));
  memcpy((char*)&szBuffer[4],(char*)&pChkPnt->Count,sizeof(short int));
  iChkCRC = ModRTU_CRC(szBuffer,6);
  szBuffer[6] = iChkCRC % 0x100;
  szBuffer[7] = iChkCRC / 0x100;
  SendMessage_ToServer(szBuffer,8);  
}

//---------------------------------------------------------------------------
// Main Controller 와의 통신프로토콜을 분석하기 위한 Parser (rs485_Serial01 - Main)
//---------------------------------------------------------------------------
void KSX3267_Receive_Message()
{
  int     i, iLength;
  unsigned short int  iChkCRC;
  RequestGetData      *pChkPnt;
  RequestWriteData    *pChkData;
  RequestWriteBlock   *pChkBlock;

  while (iRecvCount[0] > 0x04) {
    LCD_Display_Recv_Message(0,16,0x01,iRecvCount[0],szRecvBuf[0]);
    if (szRecvBuf[0][1] == 0x03) {   // OpCode = 0x03  (Read Holding Register)
      if (iRecvCount[0] == 0x08) {
        iChkCRC = ModRTU_CRC(szRecvBuf[0], 0x06);
        if (szRecvBuf[0][6] == iChkCRC % 0x100 && szRecvBuf[0][7] == iChkCRC / 0x100) {
          pChkPnt = (RequestGetData*)&szRecvBuf[0][2];
//          Serial.println("  >>> Protocol_Request_DataBlock");
//          Serial_Display_DebugMessage(0,"Master",szRecvBuf[0],iRecvCount[0]);
          Protocol_Request_DataBlock(szRecvBuf[0][0], Convert_Endian(pChkPnt->StartAddr), Convert_Endian(pChkPnt->iCount));
        }
        iRecvCount[0] = iRecvCount[0] - 0x08;
        if (iRecvCount[0] > 0) memcpy(szRecvBuf[0], &szRecvBuf[0][0x08], iRecvCount[0]);
        else iRecvCount[0] = 0;
      } else if (iRecvCount[0] > 0x08) {
        iLength = szRecvBuf[0][2];
        iChkCRC = ModRTU_CRC(szRecvBuf[0], iLength + 3);
        if (szRecvBuf[0][iLength + 3] == (iChkCRC / 0x100) && szRecvBuf[0][iLength + 4] == (iChkCRC % 0x100)) {
        }
        iRecvCount[0] = iRecvCount[0] - iLength + 5;
        if (iRecvCount[0] > 0) memcpy(szRecvBuf[0], &szRecvBuf[0][iLength + 5], iRecvCount[0]);
        else iRecvCount[0] = 0;
      } else return;
    } else if (szRecvBuf[0][1]==0x06) {
      iChkCRC = ModRTU_CRC(szRecvBuf[0],0x06);
      if (szRecvBuf[0][6]==iChkCRC%0x100 && szRecvBuf[0][7]==iChkCRC/0x100) {
        pChkData = (RequestWriteData*)&szRecvBuf[0][2];
        Protocol_Write_DataBlock(szRecvBuf[0][0],Convert_Endian(pChkData->Addr),Convert_Endian(pChkData->Data));
      }
      iRecvCount[0] = iRecvCount[0] - 0x08;
      if (iRecvCount[0] > 0) memcpy(szRecvBuf[0],&szRecvBuf[0][0x08],iRecvCount[0]);
       else iRecvCount[0] = 0;
    } else if (szRecvBuf[0][1]==0x10) {
      pChkBlock = (RequestWriteBlock*)&szRecvBuf[0];
      iChkCRC = ModRTU_CRC(szRecvBuf[0],pChkBlock->Length+7);
      if (szRecvBuf[0][pChkBlock->Length+7]==iChkCRC%0x100 && szRecvBuf[0][pChkBlock->Length+8]==iChkCRC/0x100) {
        Protocol_MultiWrite_DataBlock(pChkBlock);
//        Serial.println("  >>> Protocol_Request_DataBlock");
//        Serial_Display_DebugMessage(0,"Master",szRecvBuf[0],iRecvCount[0]);
      }
      iRecvCount[0] = iRecvCount[0] - (pChkBlock->Length+9);
      if (iRecvCount[0] > 0) memcpy(szRecvBuf[0],&szRecvBuf[0][(pChkBlock->Length+9)],iRecvCount[0]);
       else iRecvCount[0] = 0;
    } else {
      iRecvCount[0] = iRecvCount[0] - 1;
      if (iRecvCount[0] > 0) memcpy(szRecvBuf[0], &szRecvBuf[0][1], iRecvCount[0]);
    }
  }
}
