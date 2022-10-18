#ifndef KSX3267_FUNCTION_H
#define KSX3267_FUNCTION_H
  int   MemoryMap_Search_byBlank();
  void  Protocol_Request_ErrorCode(int SlaveID,int CmdCode,int iErrorCode);
  void  Protocol_Request_DataBlock(int SlaveID, int StartAddr, int iCount);
  void  Protocol_Write_DataBlock(int SlaveID, int Addr, int Data);
  void  Protocol_MultiWrite_DataBlock(RequestWriteBlock *pChkPnt);
  void  KSX3267_Receive_Message();
#endif
