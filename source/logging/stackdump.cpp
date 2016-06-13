//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma comment(lib, "Dbghelp.lib")

#include <iomanip>
#include <sstream>
#include <memory>
#include <vector>

#include <rapid/platform/platform.h>

#include <Dbghelp.h>

#include <rapid/logging/stackdump.h>

namespace rapid {

namespace logging {

namespace details {

static size_t const MAX_STACK_FRAME_DEPTH = 64;
static uint32_t const EXCEPTION_CPP = 0xE06D7363;

#define MAKE_CODE_AND_NAME(x) {x, #x}

static struct {
    uint32_t code;
    char const * name;
} const KnownExceptionNameMap[] = {
    MAKE_CODE_AND_NAME(EXCEPTION_ACCESS_VIOLATION),
    MAKE_CODE_AND_NAME(EXCEPTION_ARRAY_BOUNDS_EXCEEDED),
    MAKE_CODE_AND_NAME(EXCEPTION_DATATYPE_MISALIGNMENT),
    MAKE_CODE_AND_NAME(EXCEPTION_FLT_DENORMAL_OPERAND),
    MAKE_CODE_AND_NAME(EXCEPTION_FLT_DIVIDE_BY_ZERO),
    MAKE_CODE_AND_NAME(EXCEPTION_FLT_INEXACT_RESULT),
    MAKE_CODE_AND_NAME(EXCEPTION_FLT_INEXACT_RESULT),
    MAKE_CODE_AND_NAME(EXCEPTION_FLT_INVALID_OPERATION),
    MAKE_CODE_AND_NAME(EXCEPTION_FLT_OVERFLOW),
    MAKE_CODE_AND_NAME(EXCEPTION_FLT_STACK_CHECK),
    MAKE_CODE_AND_NAME(EXCEPTION_FLT_UNDERFLOW),
    MAKE_CODE_AND_NAME(EXCEPTION_ILLEGAL_INSTRUCTION),
    MAKE_CODE_AND_NAME(EXCEPTION_IN_PAGE_ERROR),
    MAKE_CODE_AND_NAME(EXCEPTION_INT_DIVIDE_BY_ZERO),
    MAKE_CODE_AND_NAME(EXCEPTION_INT_OVERFLOW),
    MAKE_CODE_AND_NAME(EXCEPTION_INVALID_DISPOSITION),
    MAKE_CODE_AND_NAME(EXCEPTION_NONCONTINUABLE_EXCEPTION),
    MAKE_CODE_AND_NAME(EXCEPTION_PRIV_INSTRUCTION),
    MAKE_CODE_AND_NAME(EXCEPTION_STACK_OVERFLOW),
    MAKE_CODE_AND_NAME(EXCEPTION_BREAKPOINT),
    MAKE_CODE_AND_NAME(EXCEPTION_SINGLE_STEP),
};

static std::string getSymbolInformation(uint64_t frameAddress) {
    std::ostringstream ostr;
    ostr << std::showbase << std::internal << std::setfill('0');
    ostr << "at [" << std::hex << std::setw(16) << frameAddress << "] ";

    DWORD64 displacement64 = 0;
    DWORD displacement = 0;
    
	std::vector<uint8_t> symbolBuffer(sizeof(SYMBOL_INFO) + MAX_SYM_NAME);
	
	auto symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer.data());
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = MAX_SYM_NAME;

#ifdef _M_X64
    IMAGEHLP_LINE64 line = { 0 };
    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
#else
    IMAGEHLP_LINE line = { 0 };
    line.SizeOfStruct = sizeof(IMAGEHLP_LINE);
#endif

    std::string lineInformation;
    std::string callInformation;
    
	if (::SymFromAddr(::GetCurrentProcess(), frameAddress, &displacement64, symbol) != FALSE) {
        if (symbol->NameLen > 0) {
            callInformation\
            .append(" ")\
            .append({ symbol->Name, symbol->NameLen });
        }
        if (::SymGetLineFromAddr(::GetCurrentProcess(), frameAddress, &displacement, &line) != FALSE) {
            lineInformation\
            .append("\t")\
            .append(line.FileName)\
            .append(" L: ");
            lineInformation\
            .append(std::to_string(line.LineNumber));
        }
    }
    ostr << lineInformation << callInformation;
    return ostr.str();
}

static void dumpStackFrame(CONTEXT const *context, std::vector<uint64_t> &frameAddress) {
    uint32_t machineType = 0;

    STACKFRAME64 frame = { 0 };
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Mode = AddrModeFlat;
#ifdef _M_X64
    frame.AddrPC.Offset = context->Rip;
    frame.AddrFrame.Offset = context->Rbp;
    frame.AddrStack.Offset = context->Rsp;
    machineType = IMAGE_FILE_MACHINE_AMD64;
#else
    frame.AddrPC.Offset = context->Eip;
    frame.AddrFrame.Offset = context->Ebp;
    frame.AddrStack.Offset = context->Esp;
    machineType = IMAGE_FILE_MACHINE_I386;
#endif

	std::vector<char> modulePath(MAX_PATH);

    for (auto &address : frameAddress) {
        if (::StackWalk64(machineType,
                          ::GetCurrentProcess(),
                          ::GetCurrentThread(),
                          &frame,
                          PVOID(context),
                          nullptr,
                          ::SymFunctionTableAccess64,
                          ::SymGetModuleBase64,
                          nullptr) != FALSE) {
	        auto moduleBase = ::SymGetModuleBase64(::GetCurrentProcess(), frame.AddrPC.Offset);
            
			if (moduleBase) {
				modulePath[0] = '\0';
                ::GetModuleFileNameA(reinterpret_cast<HINSTANCE>(moduleBase), modulePath.data(), MAX_PATH);
            }
            address = frame.AddrPC.Offset;
        } else {
            break;
        }
    }
}

static std::string stackFrameToText(std::vector<uint64_t> &frameAddress) {
    std::string text("Stackdump\r\n");
    for (auto const &address : frameAddress) {
        if (!address) {
            break;
        }
        text.append(getSymbolInformation(address))
        .append("\r\n");
    }
    return text;
}

std::string logStackInfo(CONTEXT const *context) {
	auto process = ::GetCurrentProcess();
	if (!::SymInitialize(process, nullptr, TRUE)) {
        return{ "SymInitialize() return failure." };
    }

	std::shared_ptr<void> autoCleanup(nullptr, [process](void*) {
		::SymCleanup(process);
    });

    std::vector<uint64_t> stackFrameAddress(MAX_STACK_FRAME_DEPTH);
    dumpStackFrame(context, stackFrameAddress);
    return stackFrameToText(stackFrameAddress);
}

std::string getStackTrace(EXCEPTION_POINTERS const *exceptionPtr) {
    return logStackInfo(exceptionPtr->ContextRecord);
}

std::string dumpStackTrace() {
    CONTEXT currentContext = { 0 };
    ::RtlCaptureContext(&currentContext);
    return logStackInfo(&currentContext);
}

bool isKnownExceptionCode(uint32_t code) {
    for (auto i = 0; i < _countof(KnownExceptionNameMap); ++i) {
        if (KnownExceptionNameMap[i].code == code) {
            return true;
        }
    }
    return false;
}

std::string exceptionCodeToString(uint32_t code) {
    for (auto i = 0; i < _countof(KnownExceptionNameMap); ++i) {
        if (KnownExceptionNameMap[i].code == code) {
            return KnownExceptionNameMap[i].name;
        }
    }
    return{ "Unknown Exception" };
}

}

}

}
