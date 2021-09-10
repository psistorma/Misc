#pragma once


class IExeCmdProgress
{
public:
    virtual ~IExeCmdProgress()
    {}
    virtual int timeOut() const
    {
        return 5;
    }
    virtual bool needTerminateExe()
    {
        return false;
    }
    virtual void onGetResult(const std::string& sStdOut, const std::string& sStdErr) = 0;
};

bool ExecCmd(const CString& sCmd, CString* pOutResult = NULL, CString* pErrResult = NULL, IExeCmdProgress* pProgress = NULL);