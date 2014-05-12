#include "stdafx.h"
#include "Decoda.h"

#include <algorithm>


HINSTANCE g_hCurrentInstanceHandle = NULL;
void SetCurrentInstanceHandle(HINSTANCE hInstance)
{
	g_hCurrentInstanceHandle = hInstance;
}

//#include <afxstat_.h>
HINSTANCE GetCurrentInstanceHandle()
{
	return g_hCurrentInstanceHandle;
//	return AfxGetStaticModuleState()->m_hCurrentInstanceHandle;
}

wxString PathForwardslashToBackslash(wxString path)
{
	wxString newPath = path;
	newPath.Replace("/","\\");
	return newPath;
}

wxString PathToFullPath(wxString path);



#include "CriticalSectionLock.h"

class CBreakPoint
{
public:
	wxString GetFile()
	{
		return m_file;
	}
	int GetLine()
	{
		return m_line;
	}
public:
	CBreakPoint()
	{
		m_line = 0;
	}
	CBreakPoint(wxString file,int line)
	{
		m_file = file;
		m_line = line;
	}
	~CBreakPoint()
	{
	}
protected:
	wxString m_file;
	int m_line;
};

class CBreakPoints
{
public:
	void Clear()
	{
		CriticalSectionLock lock(m_criticalSection);

		m_breakpoints.clear();
	}
	void AddBreakPoint(wxString file,int line)
	{
		CriticalSectionLock lock(m_criticalSection);

		m_breakpoints.push_back(CBreakPoint(file,line));
	}
	void RemoveBreakPoint(wxString file,int line)
	{
		CriticalSectionLock lock(m_criticalSection);

		std::vector<CBreakPoint>::iterator iter;
		for(iter = m_breakpoints.begin(); iter != m_breakpoints.end(); )
		{
			bool bErase = false;
			if((file.CmpNoCase((*iter).GetFile()) == 0) && (line == (*iter).GetLine()))
			{
				iter = m_breakpoints.erase(iter);
				bErase = true;
			}
			if(!bErase)
			{
				iter++;
			}
		}
	}
	void GetLines(wxString file,std::vector<int> &lines)
	{
		CriticalSectionLock lock(m_criticalSection);

		lines.clear();

		for(unsigned i=0; i<m_breakpoints.size(); i++)
		{
			if(file.CmpNoCase(m_breakpoints[i].GetFile()) == 0)
			{
				lines.push_back(m_breakpoints[i].GetLine());
			}
		}
	}
public:
	CBreakPoints(){}
	~CBreakPoints(){}
protected:
	std::vector<CBreakPoint> m_breakpoints;

	mutable CriticalSection m_criticalSection;
};

CBreakPoints g_initBreakPoints;

CEventList g_eventList;
HANDLE g_eventThread = NULL;
Project *m_project = new Project;
unsigned int m_vm = 0;
std::vector<unsigned int> m_vms;
unsigned int m_stackLevel = 0;
DWORD g_dwThreadId = GetCurrentThreadId();

wxString g_command;
wxString g_commandArguments;
wxString g_workingDirectory;
wxString g_symbolsDirectory;

CallbackEventInitialize g_CallbackEventInitialize = NULL;
CallbackEventCreateVM g_CallbackEventCreateVM = NULL;
CallbackEventDestroyVM g_CallbackEventDestroyVM = NULL;
CallbackEventLoadScript g_CallbackEventLoadScript = NULL;
CallbackEventBreak g_CallbackEventBreak = NULL;
CallbackEventSetBreakpoint g_CallbackEventSetBreakpoint = NULL;
CallbackEventException g_CallbackEventException = NULL;
CallbackEventLoadError g_CallbackEventLoadError = NULL;
CallbackEventMessage g_CallbackEventMessage = NULL;
CallbackEventSessionEnd g_CallbackEventSessionEnd = NULL;
CallbackEventNameVM g_CallbackEventNameVM = NULL;

DWORD WINAPI EventThreadProc(LPVOID param);

#include <Shlobj.h>
wxString GetBabeLuaDirectory()
{
	TCHAR szDocumentPath[MAX_PATH] = {0};
	SHGetSpecialFolderPath(NULL,szDocumentPath,CSIDL_PERSONAL,0);
	wxString documentPath = szDocumentPath;
	documentPath += "\\BabeLua";
	return documentPath;
}
#pragma comment(lib,"Version.lib")

bool GetApplicationVersion(HMODULE hModule,WORD nProdVersion[4])
{
	TCHAR szFullPath[MAX_PATH];
	DWORD dwVerInfoSize = 0;
	DWORD dwVerHnd;
	VS_FIXEDFILEINFO * pFileInfo;

	GetModuleFileName(hModule, szFullPath, sizeof(szFullPath));
	dwVerInfoSize = GetFileVersionInfoSize(szFullPath, &dwVerHnd);
	if (dwVerInfoSize)
	{
		// If we were able to get the information, process it:
		HANDLE  hMem;
		LPVOID  lpvMem;
		unsigned int uInfoSize = 0;

		hMem = GlobalAlloc(GMEM_MOVEABLE, dwVerInfoSize);
		lpvMem = GlobalLock(hMem);
		GetFileVersionInfo(szFullPath, dwVerHnd, dwVerInfoSize, lpvMem);

		::VerQueryValue(lpvMem, (LPTSTR)_T("\\"), (void**)&pFileInfo, &uInfoSize);

		// File version from the FILEVERSION of the version info resource 
		nProdVersion[0] = HIWORD(pFileInfo->dwFileVersionMS); 
		nProdVersion[1] = LOWORD(pFileInfo->dwFileVersionMS);
		nProdVersion[2] = HIWORD(pFileInfo->dwFileVersionLS);
		nProdVersion[3] = LOWORD(pFileInfo->dwFileVersionLS); 

		GlobalUnlock(hMem);
		GlobalFree(hMem);

		return true;
	}
	return false;
}

#include <dbghelp.h>
#pragma comment(lib,"dbghelp.lib")
#include <Shellapi.h>
LONG WINAPI CrashFunction(__in struct _EXCEPTION_POINTERS *ExceptionInfo)
{
	WORD nProdVersion[4] = {0};
	GetApplicationVersion(GetCurrentInstanceHandle(),nProdVersion);

	wxString version = wxString::Format("%d.%d.%d.%d",nProdVersion[0],nProdVersion[1],nProdVersion[2],nProdVersion[3]);

	wxString dumpFilePath = GetBabeLuaDirectory()+"\\dumpfile V"+version+".dmp";
	HANDLE hFile = ::CreateFile(dumpFilePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if( hFile != INVALID_HANDLE_VALUE)
	{
		MINIDUMP_EXCEPTION_INFORMATION einfo;
		einfo.ThreadId = ::GetCurrentThreadId();
		einfo.ExceptionPointers = ExceptionInfo;
		einfo.ClientPointers = FALSE;

		::MiniDumpWriteDump(::GetCurrentProcess(), ::GetCurrentProcessId(), hFile, MiniDumpNormal, &einfo, NULL, NULL);
		::CloseHandle(hFile);

		wxString info = wxString::Format("error file: \r\n%s\r\n\r\nplease send the file to us help us improving.\r\n\r\npress ok to open the file directory.",dumpFilePath);
		MessageBox(NULL,info,"error",MB_OK);
		ShellExecute(NULL,NULL,"explorer","/select, "+dumpFilePath,NULL,SW_SHOW);
	}
	return 0;
}

class CInitDistory
{
public:
	CInitDistory()
	{
		SetUnhandledExceptionFilter(CrashFunction);

		DebugFrontend::Get().SetEventList(&g_eventList);

//		g_eventThread = CreateThread(NULL, 0, EventThreadProc, &g_eventList, 0, &g_dwThreadId);
	}
	~CInitDistory()
	{
		if(m_project != NULL)
		{
			delete m_project;
			m_project = NULL;
		}

		DebugFrontend::Destroy();
	}
};
CInitDistory g_initdistory;


DWORD WINAPI EventThreadProc(LPVOID param)
{
    CEventList *pEventList = static_cast<CEventList*>(param);
	if(pEventList == NULL)
		return -1;

	while(TRUE)
	{
		wxDebugEvent *pEvent = (wxDebugEvent*)pEventList->PopEvent();
		if(pEvent != NULL)
		{
//			OnDebugEvent(*pEvent);

			delete pEvent;
			pEvent = NULL;
		}

		Sleep(1);
	}

	return 0;
}

Project::File* GetFileMatchingSource(const wxFileName& fileName, const std::string& source)
{
	if(m_project == NULL)
		return NULL;

    for (unsigned int i = 0; i < m_project->GetNumFiles(); ++i)
    {
        Project::File* file = m_project->GetFile(i);
        if(file == NULL)
			continue;

        if (file->scriptIndex == -1 && file->fileName.GetFullName().CmpNoCase(fileName.GetFullName()) == 0)
        {
            return file;
        }
    }
    return NULL;
}


void SetCallbackEventInitialize(CallbackEventInitialize callbackFunction)
{
	g_CallbackEventInitialize = callbackFunction;
}

void SetCallbackEventCreateVM(CallbackEventCreateVM callbackFunction)
{
	g_CallbackEventCreateVM = callbackFunction;
}

void SetCallbackEventDestroyVM(CallbackEventDestroyVM callbackFunction)
{
	g_CallbackEventDestroyVM = callbackFunction;
}

void SetCallbackEventLoadScript(CallbackEventLoadScript callbackFunction)
{
	g_CallbackEventLoadScript = callbackFunction;
}

void SetCallbackEventBreak(CallbackEventBreak callbackFunction)
{
	g_CallbackEventBreak = callbackFunction;
}

void SetCallbackEventSetBreakpoint(CallbackEventSetBreakpoint callbackFunction)
{
	g_CallbackEventSetBreakpoint = callbackFunction;
}

void SetCallbackEventException(CallbackEventException callbackFunction)
{
	g_CallbackEventException = callbackFunction;
}

void SetCallbackEventLoadError(CallbackEventLoadError callbackFunction)
{
	g_CallbackEventLoadError = callbackFunction;
}

void SetCallbackEventMessage(CallbackEventMessage callbackFunction)
{
	g_CallbackEventMessage = callbackFunction;
}

void SetCallbackEventSessionEnd(CallbackEventSessionEnd callbackFunction)
{
	g_CallbackEventSessionEnd = callbackFunction;
}

void SetCallbackEventNameVM(CallbackEventNameVM callbackFunction)
{
	g_CallbackEventNameVM = callbackFunction;
}


void SetContext(unsigned int vm, unsigned int stackLevel)
{
    m_vm = vm;
    m_stackLevel = stackLevel;
/*
    m_watch->SetContext(m_vm, m_stackLevel);
    m_watch->UpdateItems();

    // Update the selection in the VM list.

    m_vmList->ClearAllIcons();
    unsigned int vmIndex = std::find(m_vms.begin(), m_vms.end(), vm) - m_vms.begin();

    if (vmIndex < m_vms.size())
    {
        m_vmList->SetItemIcon(vmIndex, ListWindow::Icon_YellowArrow);
    }

    // Update the icons in the call stack.

    m_callStack->ClearAllIcons();

    if (stackLevel < static_cast<unsigned int>(m_callStack->GetItemCount()))
    {
        m_callStack->SetItemIcon(0, ListWindow::Icon_YellowArrow);
        if (stackLevel != 0)
        {
            m_callStack->SetItemIcon(stackLevel, ListWindow::Icon_GreenArrow);
        }
    }
*/
}

void CleanUpTemporaryFiles()
{
    std::vector<Project::File*> files;

    for (unsigned int i = 0; i < m_project->GetNumFiles(); ++i)
    {
        Project::File* file = m_project->GetFile(i);
		if(file == NULL)
			continue;

        if (file->temporary /*&& GetOpenFileIndex(file) == -1*/)
        {
            files.push_back(file);
        }
    }
//    m_projectExplorer->RemoveFiles(files);

    for (unsigned int i = 0; i < files.size(); ++i)
    {
        m_project->RemoveFile(files[i]);
    }
//    m_breakpointsWindow->UpdateBreakpoints();
}

unsigned int OldToNewLine(Project::File* file, unsigned int oldLine)
{
	if(file == NULL)
		return oldLine;

    DebugFrontend::Script* script = DebugFrontend::Get().GetScript(file->scriptIndex);
    wxASSERT(script != NULL);

    if (script == NULL)
    {
        // This file isn't being debugged, so we don't need to map lines.
        return oldLine;
    }
    else
    {
        return script->lineMapper.GetNewLine( oldLine );
    }
}

void UpdateScriptLineMappingFromFile(const Project::File* file, DebugFrontend::Script* script)
{
	if(file == NULL)
		return;

    if (file->fileName.FileExists())
    {
        // Read the file from disk.
        wxFile diskFile(file->fileName.GetFullPath());

        if (diskFile.IsOpened())
        {
            unsigned int diskFileSize = file->fileName.GetSize().GetLo();
            char* diskFileSource = new char[diskFileSize + 1];

            diskFileSize = diskFile.Read(diskFileSource, diskFileSize);
            diskFileSource[diskFileSize] = 0; 
            
            script->lineMapper.Update(script->source, diskFileSource);

            delete [] diskFileSource;
            diskFileSource = NULL;
        }
    }
}

void RemoveAllLocalBreakpoints(Project::File* file)
{
	if(file == NULL)
		return;
/*
    unsigned int openIndex = GetOpenFileIndex(file);

    if (openIndex != -1)
    {
        for (unsigned int i = 0; i < file->breakpoints.size(); ++i)
        {
            UpdateFileBreakpoint(m_openFiles[openIndex], file->breakpoints[i], false);
        }
    }
*/
    file->breakpoints.clear();
//    m_breakpointsWindow->UpdateBreakpoints(file);
}

class CWriteDebugEvent
{
public:
	void WriteDebugEvent(const wxDebugEvent& event)
	{
		if(!m_bWrite)
		{
			wxString log = GetDebugEvent(event);
			WriteDecodaLog(log);
		}
		m_bWrite = TRUE;
	}
	void WriteDebugEvent(const wxDebugEvent& event,const wxString &file)
	{
		if(!m_bWrite)
		{
			wxString log = GetDebugEvent(event)+" "+wxString::Format("file=%s",file.c_str());
			WriteDecodaLog(log);
		}
		m_bWrite = TRUE;
	}
private:
	wxString GetDebugEvent(const wxDebugEvent& event)
	{
		wxString debugEvent = wxString::Format("eventId=%d vm=%d scriptIndex=%d line=%d enabled=%d message=%s messageType=%d",
			event.GetEventId(),event.GetVm(),event.GetScriptIndex(),event.GetLine(),
			event.GetEnabled(),event.GetMessage(),event.GetMessageType());
		return debugEvent;
	}
public:
	CWriteDebugEvent()
	{
		m_bWrite = FALSE;
	}
	~CWriteDebugEvent()
	{
	}
protected:
	BOOL m_bWrite;
};

void OutputMessage(int msgType,const char *msg)
{
	if(g_CallbackEventMessage != NULL)
	{
		g_CallbackEventMessage(g_dwThreadId,msgType,"",0,msg);
	}
}

void OutputMessage(int msgType,const char *fullPath,int line,const char *msg)
{
	if(g_CallbackEventMessage != NULL)
	{
		g_CallbackEventMessage(g_dwThreadId,msgType,::PathToFullPath(fullPath),line,msg);
	}
}

void AddVmToList(unsigned int vm)
{
    assert(std::find(m_vms.begin(), m_vms.end(), vm) == m_vms.end());
    m_vms.push_back(vm);
/*
    char vmText[256];
    sprintf(vmText, "0x%08x", vm);

    m_vmList->Append(vmText);*/
}

void RemoveVmFromList(unsigned int vm)
{
    std::vector<unsigned int>::iterator iterator;
    iterator = std::find(m_vms.begin(), m_vms.end(), vm);

    if (iterator != m_vms.end())
    {
//        unsigned int index = iterator - m_vms.begin();
//        m_vmList->DeleteItem(index);
        m_vms.erase(iterator);
    }
}

bool ParseHelpMessage(const wxString& message, wxString& topic)
{
    int topicId;
    if (sscanf(message.c_str(), "Warning %d :", &topicId) == 1)
    {
        topic.Printf("Decoda_warning_%d.html", topicId);
        return true;
    }
    else if (sscanf(message.c_str(), "Error %d :", &topicId) == 1)
    {
        topic.Printf("Decoda_error_%d.html", topicId);
        return true;
    }
    return false;
}

bool ParseLuaErrorMessage(const wxString& error, wxString& fileName, unsigned int& line, wxString& message)
{
    // Error messages have the form "filename:line: message"
    fileName = error;
    fileName.Trim(false);

    int fileNameEnd;
    if (fileName.Length() >= 3 && isalpha(fileName[0]) && fileName[1] == ':' && wxIsPathSeparator(fileName[2]))
    {
        // The form appears to have a drive letter in front of the path.
        fileNameEnd = fileName.find(':', 3);
    }
    else
    {
        fileNameEnd = fileName.find(':');
    }

    if (fileNameEnd == wxNOT_FOUND)
    {
        return false;
    }

	message.resize(error.Length()+1);
    if (sscanf(fileName.c_str() + fileNameEnd, ":%d:%[^\0]", &line, message) >= 1)
    {
        fileName = fileName.Left(fileNameEnd);
        return true;
    }
    return false;
}

bool ParseLuacErrorMessage(const wxString& error, wxString& fileName, unsigned int& line, wxString& message)
{
    // "appname: filename:line: message"
    int appNameEnd = error.Find(wxT(": "));
    if (appNameEnd == wxNOT_FOUND)
    {
        return false;
    }

    wxString temp = error.Right(error.Length() - appNameEnd - 1);
    return ParseLuaErrorMessage(temp, fileName, line, message);
}

bool ParseErrorMessage(const wxString& error, wxString& fileName, unsigned int& line, wxString& message)
{
    if (ParseLuaErrorMessage (error, fileName, line, message) ||
        ParseLuacErrorMessage(error, fileName, line, message))
    {
		// Check if the target stars with "...". Luac does this if the file name is too long.
        // In that case, we find the closest matching file in the project.
        if ( fileName.StartsWith(wxT("...")) )
        {
            bool foundMatch = false;
            wxString partialName = wxFileName(fileName.Mid(3)).GetFullPath();

            for (unsigned int fileIndex = 0; fileIndex < m_project->GetNumFiles() && !foundMatch; ++fileIndex)
            {
                Project::File* file = m_project->GetFile(fileIndex);
                wxString fullName = file->fileName.GetFullPath();

                if (fullName.EndsWith(partialName))
                {
                    fileName = fullName;
                    foundMatch = true;
                }
            }
        }
        return true;
    }
    return false;
}

bool ParseMessage(const wxString& error, wxString& target, unsigned int& line,wxString &message)
{
	if (ParseHelpMessage(error, target))
	{
//		m_helpController.DisplaySection(target);
		target = "";
		line = 0;
		message = error;

		return true;
	}
	else if (ParseErrorMessage(error, target, line, message))
	{
		return true;
	}
	return false;
}

//如果是相对路径文件则在前面加上工作目录
wxString GetFileFullPath(Project::File *file)
{
	wxString wxFullPath;
	if(file != NULL)
	{
		if(file->fileName.IsRelative())
		{
			wxString workingDirectory = g_workingDirectory;
			if(workingDirectory.Right(1) != "\\")
				workingDirectory += "\\";

			wxFullPath = workingDirectory+file->fileName.GetFullPath();
		}
		else
			wxFullPath = file->fileName.GetFullPath();
	}
	return PathToFullPath(wxFullPath);
}

#include <Shlwapi.h>
#pragma comment(lib,"Shlwapi.lib")
wxString PathToFullPath(wxString path)
{
	if(::PathIsRelative(path.c_str()))
	{
		wxString workingDirectory = g_workingDirectory;
		if(workingDirectory.Right(1) != "\\")
			workingDirectory += "\\";

		path = workingDirectory+path;
	}

	TCHAR szFullPath[MAX_PATH];
	PTCHAR pFullPath = (PTCHAR)szFullPath;
	TCHAR **lppPart = &pFullPath;
	::GetFullPathName(path.c_str(),path.Length()+1,szFullPath,lppPart);
	
	wxString fullPath = szFullPath;
	return fullPath;
}

void InitSetBreakpoint(const char *fullPath,int line);

void OnDebugEvent(wxDebugEvent& event)
{
    char vmText[256];
    sprintf(vmText, "0x%08x: ", event.GetVm());

	CWriteDebugEvent writeDebugEvent;

    switch (event.GetEventId())
    {
    case EventId_LoadScript:
        {
            // Sync up the breakpoints for this file.
            unsigned int scriptIndex = event.GetScriptIndex();

            Project::File* file = m_project->GetFileForScript(scriptIndex);
            if (file == NULL)
            {
                // Check to see if one of the existing files' contents match this script.
                DebugFrontend::Script* script = DebugFrontend::Get().GetScript(scriptIndex);
                file = GetFileMatchingSource( wxFileName(DebugFrontend::Get().GetScript(scriptIndex)->name), script->source );
            
                if (file != NULL)
                {
                    // Map lines in case the loaded script is different than what we have on disk.
                    UpdateScriptLineMappingFromFile(file, script);
                }
            }

            if (file == NULL)
            {
                // Add the file to the project as a temporary file so that it's
                // easy for the user to add break points.
                file = m_project->AddTemporaryFile(scriptIndex);
                file->type = "Lua";
//                UpdateForNewFile(file);
            }
            else
            {
                // Check that we haven't already assigned this guy an index. If
                // we have, overwriting it will cause the previous index to no
                // longer exist in our project.
                assert(file->scriptIndex == -1);
            }

            if (file != NULL)
            {
                // The way this mechanism works, the front end sends all of the breakpoints
                // to the backend, and then the backend sends back commands to enable breakpoints
                // on the valid lines. So, we make a temporary copy of the breakpoints and clear
                // out our stored breakpoints before proceeding.

                std::vector<unsigned int> breakpoints = file->breakpoints;
                RemoveAllLocalBreakpoints(file);

                DebugFrontend::Script* script = DebugFrontend::Get().GetScript(scriptIndex);
                
                file->scriptIndex = scriptIndex;
/*
                for (unsigned int i = 0; i < breakpoints.size(); ++i)
                {
                    unsigned int newLine = breakpoints[i];
                    unsigned int oldLine = script->lineMapper.GetOldLine(newLine);
                    // If a line is changed, the breakpoint will be removed.
                    // Note, since we removed all breakpoints from the file already, if we
                    // don't add a breakpoint back it will be automatically deleted.
                    if (oldLine != LineMapper::s_invalidLine)
                    {
                        DebugFrontend::Get().ToggleBreakpoint(event.GetVm(), scriptIndex, oldLine);
                    }
                }
*/
				std::vector<int> lines;
				g_initBreakPoints.GetLines(::GetFileFullPath(file),lines);
				if(lines.size() > 0)
				{
					WriteDecodaLog(wxString::Format("InitBreakPoints:%s",file->fileName.GetFullPath().c_str()));
				}
				for(unsigned int i=0; i<lines.size(); i++)
				{
					InitSetBreakpoint(::GetFileFullPath(file),lines[i]);
				}

				if(g_CallbackEventLoadScript != NULL)
				{
					g_CallbackEventLoadScript(g_dwThreadId,::GetFileFullPath(file),file->scriptIndex);
				}

				writeDebugEvent.WriteDebugEvent(event,file->fileName.GetFullPath());
            }

            // Tell the backend we're done processing this script for loading.
            DebugFrontend::Get().DoneLoadingScript(event.GetVm());

        }
        break;
    case EventId_CreateVM:
		{
			if(g_CallbackEventCreateVM != NULL)
			{
				g_CallbackEventCreateVM(g_dwThreadId,event.GetVm());
			}
			OutputMessage(MessageType_Normal,wxString::Format("%sVM created", vmText));
//        m_output->OutputMessage(wxString::Format("%sVM created", vmText));
	        AddVmToList(event.GetVm());
		}
        break;
    case EventId_DestroyVM:
		{
			if(g_CallbackEventDestroyVM != NULL)
			{
				g_CallbackEventDestroyVM(g_dwThreadId,event.GetVm());
			}
			OutputMessage(MessageType_Normal,wxString::Format("%sVM destroyed", vmText));
//        m_output->OutputMessage(wxString::Format("%sVM destroyed", vmText));
	        RemoveVmFromList(event.GetVm());
		}
        break;
    case EventId_Break:
		{
			unsigned int scriptIndex = event.GetScriptIndex();
			unsigned int breakLine   = event.GetLine();
			Project::File* file = m_project->GetFileForScript(scriptIndex);
			if(file != NULL)
			{
				if(g_CallbackEventBreak != NULL)
				{
					g_CallbackEventBreak(g_dwThreadId,::GetFileFullPath(file),breakLine);
				}
				writeDebugEvent.WriteDebugEvent(event,file->fileName.GetFullPath());
			}
/*
			UpdateForNewState();

			// Bring ourself to the top of the z-order.
			BringToFront();

			ClearBreakLineMarker();

			m_breakScriptIndex  = event.GetScriptIndex();
			m_breakLine         = event.GetLine();
*/
			unsigned int stackLevel = 0;

			// Set the VM the debugger is working with to the one that this event came
			// from. Note this will update the watch values.
			SetContext(event.GetVm(), stackLevel);
		}
        break;
    case EventId_SetBreakpoint:
        {
            unsigned int scriptIndex = event.GetScriptIndex();
            Project::File* file = m_project->GetFileForScript(scriptIndex);
            unsigned int newLine = OldToNewLine(file, event.GetLine());
//            m_project->SetBreakpoint(scriptIndex, newLine, event.GetEnabled());

			if(file != NULL)
			{
				if(g_CallbackEventSetBreakpoint != NULL)
				{
					g_CallbackEventSetBreakpoint(g_dwThreadId,::GetFileFullPath(file),newLine,event.GetEnabled());
				}
				writeDebugEvent.WriteDebugEvent(event,file->fileName.GetFullPath());
			}
        }
        break;
    case EventId_Exception:
        {
			wxString target;
			unsigned int line;
			wxString error;
			if(ParseMessage(event.GetMessage(),target,line,error))
			{
				if(g_CallbackEventException != NULL)
				{
					g_CallbackEventException(g_dwThreadId,::PathToFullPath(target),line-1,error);
				}
			}
			else
			{
				if(g_CallbackEventException != NULL)
				{
					g_CallbackEventException(g_dwThreadId,"",0,event.GetMessage());
				}
			}
/*
            // Add the exception to the output window.
            m_output->OutputError(event.GetMessage());

            // Check if we're ignoring this exception.

            ExceptionDialog dialog(this, event.GetMessage(), true);
            int result = dialog.ShowModal();

            if (result == ExceptionDialog::ID_Ignore)
            {
                // Resume the backend.
                DebugFrontend::Get().Continue(m_vm);
                UpdateForNewState();
            }
            else if (result == ExceptionDialog::ID_IgnoreAlways)
            {
                DebugFrontend::Get().IgnoreException(event.GetMessage().ToAscii());
                DebugFrontend::Get().Continue(m_vm);
                UpdateForNewState();
            }
*/            
        }
        break;
    case EventId_LoadError:
		{
			wxString target;
			unsigned int line;
			wxString error;
			if(ParseMessage(event.GetMessage(),target,line,error))
			{
				if(g_CallbackEventLoadError != NULL)
				{
					g_CallbackEventLoadError(g_dwThreadId,::PathToFullPath(target),line-1,error);
				}
			}
			else
			{
				if(g_CallbackEventLoadError != NULL)
				{
					g_CallbackEventLoadError(g_dwThreadId,"",0,event.GetMessage());
				}
			}
		}
        break;
    case EventId_SessionEnd:
		{
/*
			ClearBreakLineMarker();
			ClearCurrentLineMarker();
*/
			// Check if all of the VMs have been closed.
			if (!m_vms.empty())
			{
				OutputMessage(MessageType_Warning,"Warning 1003: Not all virtual machines were destroyed");
//				m_output->OutputWarning("Warning 1003: Not all virtual machines were destroyed");
			}

			if(g_CallbackEventSessionEnd != NULL)
			{
				g_CallbackEventSessionEnd(g_dwThreadId);
			}

			m_project->CleanUpAfterSession();
			CleanUpTemporaryFiles();

			// Clean up after the debugger.
			DebugFrontend::Get().Shutdown();

//			UpdateForNewState();

			SetContext(0, 0);
			m_vms.clear();
/*			m_vmList->DeleteAllItems();

			SetMode(Mode_Editing);
			m_output->OutputMessage("Debugging session ended");
*/
			OutputMessage(MessageType_Normal,"Debugging session ended");
		}
        break;
    case EventId_Message:
		{
			wxString target;
			unsigned int line;
			wxString error;
			if(ParseMessage(event.GetMessage(),target,line,error))
			{
				OutputMessage(event.GetMessageType(),target,line-1,error);
			}
			else
			{
				OutputMessage(event.GetMessageType(),event.GetMessage());
			}
		}
//        OnMessage(event);
        break;
    case EventId_NameVM:
		{
			if(g_CallbackEventNameVM != NULL)
			{
				g_CallbackEventNameVM(g_dwThreadId,event.GetVm(),event.GetMessage());
			}
		}
//        SetVmName(event.GetVm(), event.GetMessage());
        break;
	default:
		break;
    }

	writeDebugEvent.WriteDebugEvent(event);
}

void StartProcess(const wxString& command, const wxString& commandArguments, const wxString& workingDirectory, const wxString& symbolsDirectory, bool debug, bool startBroken)
{
    if (!DebugFrontend::Get().Start(command, commandArguments, workingDirectory, symbolsDirectory, debug, startBroken))
    {
		OutputMessage(MessageType_Error,"Error starting process");
//        wxMessageBox("Error starting process", s_applicationName, wxOK | wxICON_ERROR, this);
    }
    else if (debug)
    {
		OutputMessage(MessageType_Normal,"Debugging session started");
/*        SetMode(Mode_Debugging);

        m_output->OutputMessage("Debugging session started");
        if (m_attachToHost)
        {
            DebugFrontend::Get().AttachDebuggerToHost();
        }*/
    }
}

void StartProcess(bool debug, bool startBroken)
{
    wxString command = g_command;
    wxString commandArguments = g_commandArguments;
    wxString workingDirectory = g_workingDirectory;
    wxString symbolsDirectory = g_symbolsDirectory;

    if (!command.IsEmpty())
    {
        StartProcess(command, commandArguments, workingDirectory, symbolsDirectory, debug, startBroken);
    }
}

BOOL CreateDirectory(wxString directory)
{
	return CreateDirectory(directory,NULL);
}

void CreateMultipleDirectory(wxString directory)
{
	if(directory.Length() <= 0)
		return;
	if(directory.Right(1) == '\\')
	{
		directory = directory.Left(directory.Length()-1);
	}
	if(GetFileAttributes(directory) != -1)
		return;

	int iIndex = directory.rfind('\\');

	CreateMultipleDirectory(directory.Left(iIndex));
	CreateDirectory(directory,NULL);
}

#include <fstream>
int WriteLog(const char *filePath,const char *log)
{
	SYSTEMTIME systemTime;
	GetLocalTime(&systemTime);

	wxString logString = wxString::Format("[%04d-%02d-%02d %02d:%02d:%02d] %s\r\n",systemTime.wYear,systemTime.wMonth,systemTime.wDay,systemTime.wHour,systemTime.wMinute, systemTime.wSecond,log);

	wxString file = filePath;
	std::ofstream output;
    output.open(file, std::ios::ate | std::ios::app | std::ios::binary);
	if(output.is_open())
	{
		output.seekp(0, std::ios::end);
		output.write(logString.c_str(),logString.Length());
		output.close();
	}
/*
	CStdioFile file;
	if(!file.Open(file,CFile::modeCreate|CFile::modeNoTruncate|CFile::modeReadWrite,NULL))
		return -1;

	file.SeekToEnd();
	file.WriteString(csLogString);
	file.Close();
*/
	return 0;
}

BOOL IsDirectory(wxString path)
{
	DWORD dwAttributes = GetFileAttributes(path);
	if(dwAttributes == -1)
		return FALSE;
	if((dwAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
		return TRUE;
	else
		return FALSE;
}

BOOL IsDirectoryExist(wxString directory)
{
	return IsDirectory(directory);
}

CriticalSection m_writeLogCriticalSection;
//CLog g_log(GetBabeLuaDirectory());
bool g_bWriteLog = false;
void SetWriteLog(int iEnable)
{
	g_bWriteLog = (iEnable != 0);
}

void SetDecodaLogMaxFileSize(int maxFileSize)
{
//	g_log.SetDecodaLogMaxFileSize(maxFileSize);
}

void SetVsLogMaxFileSize(int maxFileSize)
{
//	g_log.SetVsLogMaxFileSize(maxFileSize);
}

int WriteDecodaLog(const char *log)
{
	CriticalSectionLock lock(m_writeLogCriticalSection);
	if(g_bWriteLog)
	{
		wxString directory = GetBabeLuaDirectory();
		if(!IsDirectoryExist(directory))
			CreateMultipleDirectory(directory);

//		g_log.DecodaLog(log);
		return 0;
	}
	return -1;
}

int WritePackageLog(const char *log)
{
	CriticalSectionLock lock(m_writeLogCriticalSection);
	if(g_bWriteLog)
	{
		wxString directory = GetBabeLuaDirectory();
		if(!IsDirectoryExist(directory))
			CreateMultipleDirectory(directory);

//		g_log.VsLog(log);
		return 0;
	}
	return -1;
}

unsigned int StartProcess(const char *command,const char *commandArguments,const char *workingDirectory,const char *symbolsDirectory)
{
	g_command = command;
	g_commandArguments = commandArguments;
	g_workingDirectory = workingDirectory;
	g_symbolsDirectory = symbolsDirectory;

	DebugStart();

	unsigned int processId = DebugFrontend::Get().GetProcessId();
	return processId;
}

void DebugStart()
{
	WriteDecodaLog("DebugStart");
    if (DebugFrontend::Get().GetState() == DebugFrontend::State_Inactive)
    {
        StartProcess(true, false);
    }
    else
    {
        // The user wants to continue since we're already running.
        DebugFrontend::Get().Continue(m_vm);
    }
}

void DebugStop()
{
	WriteDecodaLog("DebugStop");
    DebugFrontend::Get().Stop(false);
}

void StepInto()
{
	WriteDecodaLog("StepInto");
    if (DebugFrontend::Get().GetState() == DebugFrontend::State_Inactive)
    {
        StartProcess(true, true);
    }
    else
    {
        DebugFrontend::Get().StepInto(m_vm);
    }
//    UpdateForNewState();
}

void StepOver()
{
	WriteDecodaLog("StepOver");
    DebugFrontend::Get().StepOver(m_vm);
//    UpdateForNewState();
}

int GetScriptIndex(const char *fullPath)
{
	if(m_project == NULL)
		return -1;

    for (unsigned int i = 0; i < m_project->GetNumFiles(); ++i)
    {
        Project::File* file = m_project->GetFile(i);
		if(file == NULL)
			continue;

		if (file->fileName.GetFullPath().CmpNoCase(fullPath) == 0)
        {
            return file->scriptIndex;
        }
		else if(file->fileName.IsRelative())
		{
			wxString wxFullPath = ::GetFileFullPath(file);
			if(wxFullPath.CmpNoCase(fullPath) == 0)
			{
				return file->scriptIndex;
			}
		}
    }
	return -1;
}

CriticalSection g_breakCriticalSection;
bool IsFileSetBreakpoint(int scriptIndex,int line)
{
	CriticalSectionLock lock(g_breakCriticalSection);

	Project::File* file = m_project->GetFileForScript(scriptIndex);
	if(file == NULL)
		return false;

    std::vector<unsigned int>::iterator iterator;
    iterator = std::find(file->breakpoints.begin(), file->breakpoints.end(), line);

    if (iterator == file->breakpoints.end())
		return false;

	return true;
}

bool FileSetBreakpoint(int scriptIndex,int line,bool set)
{
	CriticalSectionLock lock(g_breakCriticalSection);

    m_project->SetBreakpoint(scriptIndex, line, set);
	DebugFrontend::Get().ToggleBreakpoint(m_vm, scriptIndex, line);

	return true;
}

//LoadScript时设置Init断点调用
void InitSetBreakpoint(const char *fullPath,int line)
{
	int scriptIndex = GetScriptIndex(fullPath);
	wxString log = wxString::Format("InitSetBreakpoint:scriptIndex=%d,line=%d fullPath=%s",scriptIndex,line,fullPath);
	WriteDecodaLog(log);
	if(scriptIndex != -1)
	{
		if(!IsFileSetBreakpoint(scriptIndex,line))
		{
			WriteDecodaLog("!IsSetBreakpoint");
            FileSetBreakpoint(scriptIndex, line, true);
//			DebugFrontend::Get().ToggleBreakpoint(m_vm, scriptIndex, line);
		}
	}
}

void SetBreakpoint(const char *fullPath,int line)
{
	int scriptIndex = GetScriptIndex(fullPath);
	wxString log = wxString::Format("SetBreakpoint:scriptIndex=%d,line=%d fullPath=%s",scriptIndex,line,fullPath);
	WriteDecodaLog(log);
	if(scriptIndex != -1)
	{
		if(!IsFileSetBreakpoint(scriptIndex,line))
		{
			WriteDecodaLog("!IsSetBreakpoint");
            FileSetBreakpoint(scriptIndex, line, true);
//			DebugFrontend::Get().ToggleBreakpoint(m_vm, scriptIndex, line);
		}
	}
	else
	{
		//如果在Project中没有找到文件，则可能是该文件尚未require，则先添加到InitBreakpoint中，待LoadScript时设置断点
		AddInitBreakpoint(fullPath,line);
	}
}

void RemoveInitBreakpoint(const char *fullPath,int line);

void DisableBreakpoint(const char *fullPath,int line)
{
	CriticalSectionLock lock(g_breakCriticalSection);

	int scriptIndex = GetScriptIndex(fullPath);
	wxString log = wxString::Format("DisableBreakpoint:scriptIndex=%d,line=%d fullPath=%s",scriptIndex,line,fullPath);
	WriteDecodaLog(log);
	if(scriptIndex != -1)
	{
		if(IsFileSetBreakpoint(scriptIndex,line))
		{
			WriteDecodaLog("IsSetBreakpoint");
            FileSetBreakpoint(scriptIndex, line, false);
//			DebugFrontend::Get().ToggleBreakpoint(m_vm, scriptIndex, line);
		}
	}
	else
	{
		//如果在Project中没有找到文件，则可能是该文件尚未require，则先从InitBreakpoint中移除
		RemoveInitBreakpoint(fullPath,line);
	}
}

int GetNumStackFrames()
{
    DebugFrontend& frontend = DebugFrontend::Get();
    unsigned int numStackFrames = frontend.GetNumStackFrames();
	return numStackFrames;
}

char* CopyString(char *dest,int destLen,const char *source)
{
	memset(dest,0,destLen);
	memcpy(dest,source,min(destLen-1,(int)strlen(source)));
	return dest;
}

void GetStackFrame(int stackFrameIndex,char *fullPath,int fullPathLen,char *fun,int funLen,int *line)
{
    DebugFrontend& frontend = DebugFrontend::Get();
    unsigned int numStackFrames = frontend.GetNumStackFrames();
	if(stackFrameIndex >= 0 && stackFrameIndex < (int)numStackFrames)
	{
        const DebugFrontend::StackFrame& stackFrame = frontend.GetStackFrame(stackFrameIndex);
        
        if (stackFrame.scriptIndex != -1)
        {
            Project::File* file = m_project->GetFileForScript(stackFrame.scriptIndex);
            unsigned int lineNumber = OldToNewLine(file, stackFrame.line);

            const DebugFrontend::Script* script = frontend.GetScript(stackFrame.scriptIndex);

			wxString name = ::PathToFullPath(script->name);

			CopyString(fullPath,fullPathLen,name);//script->name.c_str());
			CopyString(fun,funLen,stackFrame.function.c_str());
			*line = lineNumber + 1;
        }
        else
        {
			CopyString(fun,funLen,stackFrame.function.c_str());
			*line = -1;
        }
    }
}

#include <wx/sstream.h>
#include <wx/xml/xml.h>
#include ".\Frontend\XmlUtility.h"

wxString GetTableAsText(wxXmlNode* root);
wxString GetNodeAsText(wxXmlNode* node, wxString& type);

wxString GetTableAsText(wxXmlNode* root)
{
    assert(root->GetName() == "table");

    int maxElements = 4;
    // Add the elements of the table as tree children.
    wxString result = "{";
    wxString type;

    wxXmlNode* node = root->GetChildren();
    int numElements = 0;
    while (node != NULL)
    {
        if (node->GetName() == "element")
        {
            wxXmlNode* keyNode  = FindChildNode(node, "key");
            wxXmlNode* dataNode = FindChildNode(node, "data");

            if (keyNode != NULL && dataNode != NULL)
            {
                wxString key  = GetNodeAsText(keyNode->GetChildren(), type);
                wxString data = GetNodeAsText(dataNode->GetChildren(), type);
                if (numElements >= maxElements)
                {
                    result += "...";
                    break;
                }
                result += key + "=" + data + " ";
                ++numElements;
            }
        }
        node = node->GetNext();
    }
    result += "}";
    return result;
}

wxString GetNodeAsText(wxXmlNode* node, wxString& type)
{
    wxString text;
    if (node != NULL)
    {
        if (node->GetName() == "error")
        {
            ReadXmlNode(node, "error", text);
        }
        else if (node->GetName() == "table")
        {
            text = GetTableAsText(node);
        }
        else if (node->GetName() == "values")
        {
            wxXmlNode* child = node->GetChildren();
            while (child != NULL)
            {
                if (!text.IsEmpty())
                {
                    text += ", ";
                }
                wxString dummy;
                text += GetNodeAsText(child, dummy);
                child = child->GetNext();
            }
        }
        else if (node->GetName() == "value")
        {
            wxXmlNode* child = node->GetChildren();
            while (child != NULL)
            {
                ReadXmlNode(child, "type", type) ||
                ReadXmlNode(child, "data", text);
                child = child->GetNext();
            }
        }
        else if (node->GetName() == "function")
        {
            unsigned int scriptIndex = -1;
            unsigned int lineNumber  = -1;

            wxXmlNode* child = node->GetChildren();
            while (child != NULL)
            {
                ReadXmlNode(child, "script", scriptIndex);
                ReadXmlNode(child, "line",   lineNumber);
                child = child->GetNext();
            }

			text = "function";
            DebugFrontend::Script* script = DebugFrontend::Get().GetScript(scriptIndex);
            if (script != NULL)
            {
                text += " defined at ";
                text += script->name;
                text += ":";
                text += wxString::Format("%d", lineNumber + 1);
            }

            type = "function";
        }
    }
    return text;
}

bool GetExpression(wxXmlNode* root,wxString &dataType,wxString &dataValue,int *expandable)
{
    wxString type;
    wxString text = GetNodeAsText(root, type);
    
    // Remove any embedded zeros in the text. This happens if we're displaying a wide
    // string. Since we aren't using wide character wxWidgets, we cant' display that
    // properly, so we just hack it for roman text.

    bool englishWideCharacter = true;
    for (unsigned int i = 0; i < text.Length(); i += 2)
    {
        if (text[i] != 0)
        {
            englishWideCharacter = false;
        }
    }

    if (englishWideCharacter)
    {
        size_t convertedLength = WideCharToMultiByte(CP_UTF8, 0, (const wchar_t*)text.c_str(), text.Length() / sizeof(wchar_t), NULL, 0, 0, 0);

        char* result = new char[convertedLength + 1]; 
        convertedLength = WideCharToMultiByte(CP_UTF8, 0, (const wchar_t*)text.c_str(), text.Length() / sizeof(wchar_t), result, convertedLength, 0, 0);

        text = wxString(result, convertedLength);
    }

	dataType = type;
	dataValue = text;

    if (root != NULL)
    {
        if (root->GetName() == "table")
        {
            wxXmlNode* node = root->GetChildren();
            while (node != NULL)
            {
                wxString typeName;
                if (ReadXmlNode(node, "type", typeName))
                {
                }
                else if (node->GetName() == "element")
                {
                    wxXmlNode* keyNode  = FindChildNode(node, "key");
                    wxXmlNode* dataNode = FindChildNode(node, "data");

                    if (keyNode != NULL && dataNode != NULL)
                    {
						*expandable = 1;
						break;
                    }
                }
                node = node->GetNext();
            }
        }
        else if (root->GetName() == "values")
        {
            wxXmlNode* node = root->GetChildren();
            while (node != NULL)
            {
				*expandable = 1;
				break;

                node = node->GetNext();
            }

        }
    }
    return true;
}

bool Evaluate(const wxString &expression,wxString &result)
{
    if (m_vm != 0)
    {
        if (!expression.empty())
        {
            std::string temp;
            bool bEvaluate = DebugFrontend::Get().Evaluate(m_vm, expression, m_stackLevel, temp);
            result = temp.c_str();
			return bEvaluate;
        }
	}
	return false;
}

bool IsValidVaralible(char ch)
{
	if(ch == '_' || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'))
		return true;
	else
		return false;
}

//修改从VS传入的expression名称 VS传入表达式中以.为分隔符
wxString ChangeExpressionName(const char *text)
{
	wxString expression = text;
	
//	expression.Replace(".[","[");	//例如：a.[1]转为a[1]

	bool bFind = false;
	wxString changeExpression;
	unsigned int i;
	for(i=0; i<expression.Length(); i++)
	{
		if(expression[i] == '.')
		{
			if(i < expression.Length()-1)
			{
				if(bFind)
				{
					changeExpression += "']";

					bFind = false;
				}

				if(::IsValidVaralible(expression[i+1]) || expression[i+1] == '[')	//例如 a.[1]不转换，在后面进行转换
				{
					changeExpression += expression[i];
				}
				else
				{
//					changeExpression += expression[i];
					changeExpression += "['";

					bFind = true;
				}
			}
			else
			{
				changeExpression += expression[i];
			}
		}
		else
		{
			changeExpression += expression[i];
		}
	}
	if(bFind)
	{
		changeExpression += "']";
	}

	changeExpression.Replace(".[","[");	//例如：a.[1]转为a[1]

	return changeExpression;
}

bool ExecuteText(int executeId,const char *text,char *type,int typeLen,char *value,int valueLen,int *expandable)
{
	wxString result;
	bool bEvaluate = Evaluate(ChangeExpressionName(text),result);
    if (!result.IsEmpty())
	{
        wxStringInputStream stream(result);
        wxXmlDocument document;

        if (document.Load(stream))
        {
			wxString dataType;
			wxString dataValue;
            GetExpression(document.GetRoot(),dataType,dataValue,expandable);

			CopyString(type,typeLen,dataType);
			CopyString(value,valueLen,dataValue);
        }
    }
	return bEvaluate;
}

std::vector<wxString> g_expressions;

void GetExpressions(wxXmlNode* root,std::vector<wxString> &expressions)
{
    if (root != NULL)
    {
        if (root->GetName() == "table")
        {
            // Add the elements of the table as tree children.
            wxXmlNode* node = root->GetChildren();
            while (node != NULL)
            {
                wxString typeName;
                if (ReadXmlNode(node, "type", typeName))
                {
//                    SetItemText(item, 2, typeName);
                }
                else if (node->GetName() == "element")
                {
                    wxXmlNode* keyNode  = FindChildNode(node, "key");
                    wxXmlNode* dataNode = FindChildNode(node, "data");
                    if (keyNode != NULL && dataNode != NULL)
                    {
                        wxString type;
                        wxString key  = GetNodeAsText(keyNode->GetChildren(), type);
						expressions.push_back(key);
//                        wxTreeItemId child = AppendItem(item, key);
//                        AddCompoundExpression(dataNode->GetChildren());
                    }
                }
                node = node->GetNext();
            }
        }
        else if (root->GetName() == "values")
        {
            wxXmlNode* node = root->GetChildren();
            unsigned int i = 1;
            while (node != NULL)
            {
				expressions.push_back(wxString::Format("%d", i));
//                wxTreeItemId child = AppendItem(item, wxString::Format("%d", i));
//                AddCompoundExpression(node);

                node = node->GetNext();
                ++i;
            }
        }
    }
}

int EnumChildrenNum(int executeId,const char *text)
{
	g_expressions.clear();

	wxString result;
	Evaluate(ChangeExpressionName(text),result);
    if (!result.IsEmpty())
	{
        wxStringInputStream stream(result);
        wxXmlDocument document;

        if (document.Load(stream))
        {
            GetExpressions(document.GetRoot(),g_expressions);
		}
	}

	return g_expressions.size();
}

void EnumChildren(int executeId,const char *text,int subIndex,char *subText,int subTextLen)
{
	if(subIndex >= 0 && subIndex < (int)g_expressions.size())
	{
		CopyString(subText,subTextLen,g_expressions[subIndex].c_str());
	}
}

int GetProjectNumFiles()
{
	return m_project->GetNumFiles();
}

void GetProjectFile(int index,char *fullPath,int fullPathLen)
{
	if(index >= 0 && index < (int)m_project->GetNumFiles())
	{
        Project::File* file = m_project->GetFile(index);
		if(file != NULL)
		{
			CopyString(fullPath,fullPathLen,::GetFileFullPath(file));
		}
	}
}

void ClearInitBreakpoints()
{
	WriteDecodaLog("ClearInitBreakpoints");
	g_initBreakPoints.Clear();
}

void AddInitBreakpoint(const char *fullPath,int line)
{
	WriteDecodaLog(wxString::Format("AddInitBreakpoint line=%d fullPath=%s",line,fullPath));
	g_initBreakPoints.AddBreakPoint(fullPath,line);
}

void RemoveInitBreakpoint(const char *fullPath,int line)
{
	WriteDecodaLog(wxString::Format("RemoveInitBreakpoint line=%d fullPath=%s",line,fullPath));
	g_initBreakPoints.RemoveBreakPoint(fullPath,line);
}
