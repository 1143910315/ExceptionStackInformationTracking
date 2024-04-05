#include <iostream>
#include <Windows.h>
#include <DbgHelp.h>
#pragma comment(lib, "DbgHelp")
using namespace std;

// 输出线程的调用堆栈信息
void PrintStackTrace(DWORD dwThreadId) {
    CONTEXT context;
    STACKFRAME64 stackFrame;
    HANDLE hThread = OpenThread(THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION, FALSE, dwThreadId);

    if (hThread == NULL) {
        std::cerr << "Failed to open thread: " << GetLastError() << std::endl;
        return;
    }

    // 挂起线程
    SuspendThread(hThread);

    // 获取线程的上下文
    context.ContextFlags = CONTEXT_FULL;
    if (!GetThreadContext(hThread, &context)) {
        std::cerr << "Failed to get thread context: " << GetLastError() << std::endl;
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
        GetCurrentProcess(), // 当前进程句柄
        hThread, // 当前线程句柄
        &stackFrame, // 堆栈帧结构体
        &context, // 线程上下文
        NULL, // 函数表
        SymFunctionTableAccess64, // 获取函数表的函数
        SymGetModuleBase64, // 获取模块基址的函数
        NULL)) // 使用默认的加载模块回调函数
    {
        // 输出堆栈帧的地址
        std::cout << "Address: " << std::hex << stackFrame.AddrPC.Offset << std::endl;
    }

    // 恢复线程
    ResumeThread(hThread);

    CloseHandle(hThread);
}

int main(int argc, char** argv) {
    // 指定需要调试的目标进程 ID
    DWORD dwProcessId = GetCurrentProcessId();

    // 附加到目标进程
    if (!DebugActiveProcess(dwProcessId)) {
        std::cerr << "Failed to attach to process: " << GetLastError() << std::endl;
        return 1;
    }

    // 等待调试事件
    DEBUG_EVENT debugEvent;
    while (WaitForDebugEvent(&debugEvent, INFINITE)) {
        // 处理暂停事件
        if (debugEvent.dwDebugEventCode == EXCEPTION_DEBUG_EVENT && debugEvent.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_BREAKPOINT) {
            // 输出线程的调用堆栈信息
            for (DWORD dwThreadId = 0; dwThreadId < debugEvent.dwThreadId; dwThreadId++) {
                PrintStackTrace(debugEvent.dwThreadId);
            }

            // 继续执行目标进程
            ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId, DBG_CONTINUE);
        }
        else {
            // 其他调试事件的处理
            // ...
        }
    }

    // 分离目标进程
    DebugActiveProcessStop(dwProcessId);

    return 0;
}