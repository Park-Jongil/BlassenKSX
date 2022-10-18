#ifndef WATERCOOL_FUNCTION_H
#define WATERCOOL_FUNCTION_H
  void SlaveDevice_ScanbyTime();
  void SlaveDevice_GetStatusbyTime();  
  void Protocol_Parser_SetSystemMode(int iDeviceID,int iPowerMode,int iSystemMode,int iSetTemp);
  void Protocol_Parser_GetID(RequestProtocol *pChkPnt);
  void Protocol_Parser_GetSystemMode(RequestProtocol *pChkPnt);
  void WaterCooller_Receive_Message(int iChannel);
#endif
