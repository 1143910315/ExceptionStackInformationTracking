#include <iostream>
#include <Windows.h>
#include <DbgHelp.h>
#include <fstream>
#pragma comment(lib, "DbgHelp")
using namespace std;
std::ofstream outfile("example.txt");
// 输出线程的调用堆栈信息
void PrintStackTrace(HANDLE hProcess, DWORD dwThreadId)
{
    CONTEXT context;
    STACKFRAME64 stackFrame;
    HANDLE hThread = OpenThread(THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION, FALSE, dwThreadId);

    if (hThread == NULL)
    {
        outfile << "Failed to open thread: " << GetLastError() << std::endl;
        return;
    }

    // 挂起线程
    SuspendThread(hThread);

    // 获取线程的上下文
    context.ContextFlags = CONTEXT_FULL;
    if (!GetThreadContext(hThread, &context))
    {
        outfile << "Failed to get thread context: " << GetLastError() << std::endl;
        CloseHandle(hThread);
        return;
    }

    // 初始化堆栈帧
    memset(&stackFrame, 0, sizeof(STACKFRAME64));
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrPC.Offset = context.Rip; // RIP 寄存器保存了指令指针的地址
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context.Rbp; // RBP 寄存器保存了当前栈帧的基址指针
    stackFrame.AddrStack.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = context.Rsp; // RSP 寄存器保存了栈顶指针

    // 输出调用堆栈信息
    while (StackWalk64(
        IMAGE_FILE_MACHINE_AMD64, // 指定是 64 位程序
        hProcess,                 // 当前进程句柄
        hThread,                  // 当前线程句柄
        &stackFrame,              // 堆栈帧结构体
        &context,                 // 线程上下文
        NULL,                     // 函数表
        SymFunctionTableAccess64, // 获取函数表的函数
        SymGetModuleBase64,       // 获取模块基址的函数
        NULL))                    // 使用默认的加载模块回调函数
    {
        // 输出堆栈帧的地址
        outfile << "Address: " << std::hex << stackFrame.AddrPC.Offset << std::endl;
    }

    // 恢复线程
    ResumeThread(hThread);

    CloseHandle(hThread);
}

int main(int argc, char **argv)
{
    // 指定需要调试的目标进程 ID
    DWORD dwProcessId = std::atol(argv[1]);
    // 获取目标进程的句柄
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwProcessId);

    // 附加到目标进程
    if (!DebugActiveProcess(dwProcessId))
    {
        std::cerr << "Failed to attach to process: " << GetLastError() << std::endl;
        return 1;
    }

    // 等待调试事件
    DEBUG_EVENT debugEvent;
    while (WaitForDebugEventEx(&debugEvent, 30000))
    {
        outfile << debugEvent.dwDebugEventCode << "|" << debugEvent.dwThreadId << std::endl;
        // 处理创建线程事件
        if (debugEvent.dwDebugEventCode == CREATE_THREAD_DEBUG_EVENT)
        {
            // 输出线程的调用堆栈信息
            for (DWORD dwThreadId = 0; dwThreadId < debugEvent.dwThreadId; dwThreadId++)
            {
                PrintStackTrace(hProcess, debugEvent.dwThreadId);
            }
        }
        else if (debugEvent.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT)
        {
            // 暂停目标进程
            //DebugBreakProcess(hProcess);
            PrintStackTrace(hProcess, debugEvent.dwThreadId);
        }
    }
    // 继续执行目标进程
    ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId, DBG_CONTINUE);

    //// 等待调试事件
    // DEBUG_EVENT debugEvent;
    // WaitForDebugEvent(&debugEvent, INFINITE);
    //// 处理暂停事件
    //
    //// 输出线程的调用堆栈信息
    // for (DWORD dwThreadId = 0; dwThreadId < debugEvent.dwThreadId; dwThreadId++)
    //{
    //     PrintStackTrace(hProcess,debugEvent.dwThreadId);
    // }
    //
    //// 继续执行目标进程
    // ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId, DBG_CONTINUE);

    // 分离目标进程
    DebugActiveProcessStop(dwProcessId);

    return 0;
}