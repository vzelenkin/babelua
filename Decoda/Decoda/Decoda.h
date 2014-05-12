#pragma once

#include <wx/wx.h>
#include <wx/file.h>

#include ".\Frontend\Project.h"
#include ".\Frontend\DebugFrontend.h"
#include ".\Frontend\DebugEvent.h"

//#include "Log.h"

void SetCurrentInstanceHandle(HINSTANCE hInstance);
HINSTANCE GetCurrentInstanceHandle();
int WriteDecodaLog(const char *log);

void OnDebugEvent(wxDebugEvent& event);


#define _EXTERN_C_  extern "C"  _declspec(dllexport)

//
typedef void (CALLBACK* CallbackEventInitialize)(int iThreadId);
typedef void (CALLBACK* CallbackEventCreateVM)(int iThreadId,int vm);
typedef void (CALLBACK* CallbackEventDestroyVM)(int iThreadId,int vm);
typedef void (CALLBACK* CallbackEventLoadScript)(int iThreadId,const char *fullPath,int scriptIndex);
typedef void (CALLBACK* CallbackEventBreak)(int iThreadId,const char *fullPath,int line);
typedef void (CALLBACK* CallbackEventSetBreakpoint)(int iThreadId,const char *fullPath,int line,int enabled);
typedef void (CALLBACK* CallbackEventException)(int iThreadId,const char *fullPath,int line,const char *msg);
typedef void (CALLBACK* CallbackEventLoadError)(int iThreadId,const char *fullPath,int line,const char *error);
typedef void (CALLBACK* CallbackEventMessage)(int iThreadId,int msgType,const char *fullPath,int line,const char *msg);
typedef void (CALLBACK* CallbackEventSessionEnd)(int iThreadId);
typedef void (CALLBACK* CallbackEventNameVM)(int iThreadId,int vm,const char *vmName);

_EXTERN_C_ void SetCallbackEventInitialize(CallbackEventInitialize callbackFunction);
_EXTERN_C_ void SetCallbackEventCreateVM(CallbackEventCreateVM callbackFunction);
_EXTERN_C_ void SetCallbackEventDestroyVM(CallbackEventDestroyVM callbackFunction);
_EXTERN_C_ void SetCallbackEventLoadScript(CallbackEventLoadScript callbackFunction);
_EXTERN_C_ void SetCallbackEventBreak(CallbackEventBreak callbackFunction);
_EXTERN_C_ void SetCallbackEventSetBreakpoint(CallbackEventSetBreakpoint callbackFunction);
_EXTERN_C_ void SetCallbackEventException(CallbackEventException callbackFunction);
_EXTERN_C_ void SetCallbackEventLoadError(CallbackEventLoadError callbackFunction);
_EXTERN_C_ void SetCallbackEventMessage(CallbackEventMessage callbackFunction);
_EXTERN_C_ void SetCallbackEventSessionEnd(CallbackEventSessionEnd callbackFunction);
_EXTERN_C_ void SetCallbackEventNameVM(CallbackEventNameVM callbackFunction);


_EXTERN_C_ void SetWriteLog(int iEnable);
_EXTERN_C_ void SetDecodaLogMaxFileSize(int maxFileSize);
_EXTERN_C_ void SetVsLogMaxFileSize(int maxFileSize);
_EXTERN_C_ int WriteLog(const char *filePath,const char *log);
_EXTERN_C_ int WritePackageLog(const char *log);
_EXTERN_C_ unsigned int StartProcess(const char *command,const char *commandArguments,const char *workingDirectory,const char *symbolsDirectory);//return processId
_EXTERN_C_ void DebugStart();
_EXTERN_C_ void DebugStop();
_EXTERN_C_ void StepInto();
_EXTERN_C_ void StepOver();
_EXTERN_C_ void SetBreakpoint(const char *fullPath,int line);
_EXTERN_C_ void DisableBreakpoint(const char *fullPath,int line);
_EXTERN_C_ int GetNumStackFrames();
_EXTERN_C_ void GetStackFrame(int stackFrameIndex,char *fullPath,int fullPathLen,char *fun,int funLen,int *line);
_EXTERN_C_ bool ExecuteText(int executeId,const char *text,char *type,int typeLen,char *value,int valueLen,int *expandable);
_EXTERN_C_ int EnumChildrenNum(int executeId,const char *text);
_EXTERN_C_ void EnumChildren(int executeId,const char *text,int subIndex,char *subText,int subTextLen);
_EXTERN_C_ int GetProjectNumFiles();
_EXTERN_C_ void GetProjectFile(int index,char *fullPath,int fullPathLen);
_EXTERN_C_ void ClearInitBreakpoints();
_EXTERN_C_ void AddInitBreakpoint(const char *fullPath,int line);
