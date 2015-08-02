/*!
    \file system.cpp
    \brief System management static class implementation
    \author Ivan Shynkarenka
    \date 06.07.2015
    \copyright MIT License
*/

#include "system.h"

#include <fstream>
#include <regex>
#include <streambuf>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#elif defined(unix) || defined(__unix) || defined(__unix__)
#include <sys/sysinfo.h>
#include <pthread.h>
#include <unistd.h>
#endif

namespace CppBenchmark {

//! @cond
namespace Internals {

#if defined(_WIN32) || defined(_WIN64)
// Helper function to count set bits in the processor mask
DWORD CountSetBits(ULONG_PTR pBitMask)
{
    DWORD dwLeftShift = sizeof(ULONG_PTR) * 8 - 1;
    DWORD dwBitSetCount = 0;
    ULONG_PTR pBitTest = (ULONG_PTR)1 << dwLeftShift;

    for (DWORD i = 0; i <= dwLeftShift; ++i)
    {
        dwBitSetCount += ((pBitMask & pBitTest) ? 1 : 0);
        pBitTest /= 2;
    }

    return dwBitSetCount;
}
#endif

} // namespace Internals
//! @endcond

std::string System::CpuArchitecture()
{
#if defined(_WIN32) || defined(_WIN64)
    HKEY hKey;
    LONG lError = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey);
    if (lError != ERROR_SUCCESS)
        return "<unknown>";

    CHAR pBuffer[_MAX_PATH] = { 0 };
    DWORD dwBufferSize = _MAX_PATH;
    lError = RegQueryValueEx(hKey, "ProcessorNameString", NULL, NULL, (LPBYTE)pBuffer, &dwBufferSize);
    if (lError != ERROR_SUCCESS)
        return "<unknown>";

    return std::string(pBuffer);
#elif defined(unix) || defined(__unix) || defined(__unix__)
    std::ifstream stream("/proc/cpuinfo");
    std::string cpuinfo((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

    static std::regex pattern("model name\\s*: (.*)");

    std::smatch matches;
    return (std::regex_match(cpuinfo, matches, pattern)) ? matches[1] : std::string("<unknown>");
#endif
}

int System::CpuLogicalCores()
{
    return CpuTotalCores().first;
}

int System::CpuPhysicalCores()
{
    return CpuTotalCores().second;
}

std::pair<int, int> System::CpuTotalCores()
{
#if defined(_WIN32) || defined(_WIN64)
    BOOL allocated = FALSE;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION pBuffer = NULL;
    DWORD dwLength = 0;

    while (!allocated) {
        DWORD dwResult = GetLogicalProcessorInformation(pBuffer, &dwLength);
        if (dwResult == FALSE) {
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                if (pBuffer != NULL)
                    free(pBuffer);
                pBuffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(dwLength);
                if (pBuffer == NULL)
                    return std::make_pair(-1, -1);
            }
            else
                return std::make_pair(-1, -1);
        }
        else
            allocated = TRUE;
    }

    std::pair<int, int> result(0, 0);
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION pCurrent = pBuffer;
    DWORD dwOffset = 0;

    while (dwOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= dwLength)
    {
        switch (pCurrent->Relationship)
        {
            case RelationProcessorCore:
                result.first += Internals::CountSetBits(pCurrent->ProcessorMask);
                result.second += 1;
                break;
            case RelationNumaNode:
            case RelationCache:
            case RelationProcessorPackage:
                break;
            default:
                return std::make_pair(-1, -1);
        }
        dwOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        pCurrent++;
    }

    free(pBuffer);

    return result;
#elif defined(unix) || defined(__unix) || defined(__unix__)
    long processors = sysconf(_SC_NPROCESSORS_ONLN);
    std::pair<int, int> result(processors, processors);
    return result;
#endif
}

int64_t System::CpuClockSpeed()
{
#if defined(_WIN32) || defined(_WIN64)
    HKEY hKey;
    long lError = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey);
    if (lError != ERROR_SUCCESS)
        return -1;

    DWORD dwMHz = 0;
    DWORD dwBufferSize = sizeof(DWORD);
    lError = RegQueryValueEx(hKey, "~MHz", NULL, NULL, (LPBYTE)&dwMHz, &dwBufferSize);
    if (lError != ERROR_SUCCESS)
        return -1;

    return dwMHz * 1000 * 1000;
#elif defined(unix) || defined(__unix) || defined(__unix__)
    std::ifstream stream("/proc/cpuinfo");
    std::string cpuinfo((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

    static std::regex pattern("cpu MHz\\s*: (.*)");

    std::smatch matches;
    return (std::regex_match(cpuinfo, matches, pattern)) ? atoi(matches[1].str().c_str()) : -1;
#endif
}

bool System::CpuHyperThreading()
{
    std::pair<int, int> cores = CpuTotalCores();
    return (cores.first != cores.second);
}

int64_t System::RamTotal()
{
#if defined(_WIN32) || defined(_WIN64)
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    return status.ullTotalPhys;
#elif defined(unix) || defined(__unix) || defined(__unix__)
    struct sysinfo si;
    return (sysinfo(&si) == 0) ? si.totalram : -1;
#endif
}

int64_t System::RamFree()
{
#if defined(_WIN32) || defined(_WIN64)
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    return status.ullAvailPhys;
#elif defined(unix) || defined(__unix) || defined(__unix__)
    struct sysinfo si;
    return (sysinfo(&si) == 0) ? si.freeram : -1;
#endif
}

int System::CurrentThreadId()
{
#if defined(_WIN32) || defined(_WIN64)
    return GetCurrentThreadId();
#elif defined(unix) || defined(__unix) || defined(__unix__)
    return pthread_self();
#endif
}

} // namespace CppBenchmark
