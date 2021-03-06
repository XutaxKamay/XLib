#include "process.h"
#include <unistd.h>

#ifdef WINDOWS
    #include <psapi.h>
#else
    #include <fcntl.h>
    #include <linux/limits.h>
#endif

using namespace XLib;

XLib::Process XLib::Process::find(const std::string& name)
{

}

auto XLib::Process::ProcessName(pid_t pid) -> std::string
{
    std::string result("unknown");

#ifndef WINDOWS
    result.reserve(PATH_MAX);

    if (readlink(
          std::string("/proc/" + std::to_string(pid) + "/exe").c_str(),
          result.data(),
          result.size())
        < 0)
    {
        throw XLIB_EXCEPTION("Could not read symlink.");
    }
#else
    result.reserve(MAX_PATH);

    auto process_handle = OpenProcess(PROCESS_QUERY_INFORMATION
                                        | PROCESS_VM_READ,
                                      false,
                                      pid);

    if (!process_handle)
    {
        throw XLIB_EXCEPTION("Could not get process handle.");
    }

    if (GetModuleFileNameExA(process_handle,
                             nullptr,
                             result.data(),
                             result.size())
        <= 0)
    {
        throw XLIB_EXCEPTION("Could not read process path.");
    }

    CloseHandle(process_handle);
#endif

    return result;
}

auto Process::self() -> Process
{
#ifdef WINDOWS
    return Process(GetCurrentProcessId());
#else
    return Process(getpid());
#endif
}

Process::Process()
 : ProcessBase(INVALID_PID), _full_name("unknown"),
   _mmap(ProcessMemoryMap(*this))
{
}

Process::Process(pid_t pid)
 : ProcessBase(pid), _full_name(ProcessName(pid)),
   _mmap(ProcessMemoryMap(*this))
{
}

auto Process::tasks() -> tasks_t
{
    return Task::list(*this);
}

auto Process::mmap() -> ProcessMemoryMap
{
    _mmap.refresh();
    return _mmap;
}
