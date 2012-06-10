#include "stdafx.h"
#include "error.h"

__declspec(noreturn) void ThrowSocketError(int err)
{
	LPSTR lpMsgBuf = "Unknown error";
	if (FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, err, 0,
			(LPSTR)&lpMsgBuf, 0, NULL))
	{
		std::string errStr = lpMsgBuf;
		LocalFree(lpMsgBuf);
		throw socket_exception(err, errStr.c_str());
	} else {
		throw socket_exception(err);
	}
}

__declspec(noreturn) void ThrowLastError(DWORD err)
{
	LPSTR lpMsgBuf = "Unknown error";
	if (FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, err, 0,
			(LPSTR)&lpMsgBuf, 0, NULL))
	{
		std::string errStr = lpMsgBuf;
		LocalFree(lpMsgBuf);
		throw lasterr_exception(err, errStr.c_str());
	} else {
		throw lasterr_exception(err);
	}
}

__declspec(noreturn) void ThrowErrnoError(int err)
{
#pragma warning(push)
#pragma warning(disable: 4996)
	throw errno_exception(err, strerror(err));
#pragma warning(pop)
}
