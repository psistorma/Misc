
#include <iostream>
#include <windows.h>
#include <atlstr.h>
#include <iostream>
#include <fstream>
#include "ExecCmd.h"

namespace
{
    void ReadFromPipe(HANDLE hPipe, std::string& sResult)
    {
        for (;;) {
            char buf[1024] = { 0 };
            DWORD dwRead = 0;
            DWORD dwAvail = 0;

            if (!::PeekNamedPipe(hPipe, NULL, 0, NULL, &dwAvail, NULL))
                break;

            if (!dwAvail) // No data available, return
                break;

            if (!::ReadFile(hPipe, buf, min(sizeof(buf) - 1, dwAvail), &dwRead, NULL) || !dwRead)
                // Error, the child process might ended
                break;

            buf[dwRead] = 0;
            sResult.append(buf);
        }
    }
}

bool ExecCmd(const CString& sCmd, CString* pOutResult, CString* pErrResult, IExeCmdProgress* pProgress)
{
    if (pOutResult) { pOutResult->Empty(); }
    if (pErrResult) { pErrResult->Empty(); }
    HANDLE hOutPipeRead = NULL, hOutPipeWrite = NULL;
    HANDLE hErrPipeRead = NULL, hErrPipeWrite = NULL;

    {
        SECURITY_ATTRIBUTES saAttr = { sizeof(SECURITY_ATTRIBUTES) };
        saAttr.bInheritHandle = TRUE; // Pipe handles are inherited by child process.
        saAttr.lpSecurityDescriptor = NULL;

        // Create a pipe to get results from child's stdout.
        if (!CreatePipe(&hOutPipeRead, &hOutPipeWrite, &saAttr, 0))
            return false;
    }
    {
        SECURITY_ATTRIBUTES saAttr = { sizeof(SECURITY_ATTRIBUTES) };
        saAttr.bInheritHandle = TRUE; // Pipe handles are inherited by child process.
        saAttr.lpSecurityDescriptor = NULL;

        // Create a pipe to get results from child's stderr.
        if (!CreatePipe(&hErrPipeRead, &hErrPipeWrite, &saAttr, 0)) {
            CloseHandle(hOutPipeWrite); CloseHandle(hOutPipeRead);
            return false;
        }
    }

    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.hStdOutput = hOutPipeWrite;
    si.hStdError = hErrPipeWrite;
    si.wShowWindow = SW_HIDE; // Prevents cmd window from flashing.
                              // Requires STARTF_USESHOWWINDOW in dwFlags.

    PROCESS_INFORMATION pi = { 0 };

    TCHAR* pCmdLine = new TCHAR[sCmd.GetLength() + 1];
    ZeroMemory(pCmdLine, sCmd.GetLength() + 1);
    _tcscpy_s(pCmdLine, sCmd.GetLength() + 1, sCmd);

    BOOL fSuccess = CreateProcess(NULL, pCmdLine, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
    delete[] pCmdLine;
    if (!fSuccess) {
        CloseHandle(hOutPipeWrite); CloseHandle(hOutPipeRead);
        CloseHandle(hErrPipeWrite); CloseHandle(hErrPipeRead);
        return false;
    }

    int nTimeOut = pProgress ? pProgress->timeOut() : 5;
    bool bRet = true;
    bool bProcessEnded = false;
    for (; !bProcessEnded;) {
        // Give some timeslice (5 ms), so we won't waste 100% CPU.
        bProcessEnded = WaitForSingleObject(pi.hProcess, nTimeOut) == WAIT_OBJECT_0;

        // Even if process exited - we continue reading, if
        // there is some data available over pipe.
        std::string sStdOut, sStdErr;
        if (pOutResult || pProgress) {
            ReadFromPipe(hOutPipeRead, sStdOut);
        }
        if (pErrResult || pProgress) {
            ReadFromPipe(hErrPipeRead, sStdErr);
        }
        if (pOutResult) { *pOutResult += CString(sStdOut.c_str()); }
        if (pErrResult) { *pErrResult += CString(sStdErr.c_str()); }
        if (pProgress) {
            pProgress->onGetResult(sStdOut, sStdErr);
            if (pProgress->needTerminateExe()) {
                UINT uExitCode = -1;
                TerminateProcess(pi.hProcess, uExitCode);
                bRet = false;
                break;
            }
        }
    } //for
    CloseHandle(hOutPipeWrite); CloseHandle(hOutPipeRead);
    CloseHandle(hErrPipeWrite); CloseHandle(hErrPipeRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return bRet;
} //ExecCmd
