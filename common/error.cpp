/*
	Copyright 2012-2013 Erik Anderson

	Licensed under the Apache License, Version 2.0 (the "License");
	you may not use this file except in compliance with the License.
	You may obtain a copy of the License at

		http://www.apache.org/licenses/LICENSE-2.0

	Unless required by applicable law or agreed to in writing, software
	distributed under the License is distributed on an "AS IS" BASIS,
	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	See the License for the specific language governing permissions and
	limitations under the License.
*/
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

__declspec(noreturn) void ThrowHRESULT(long err)
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
		throw hresult_exception(err, errStr.c_str());
	} else {
		throw hresult_exception(err);
	}
}

__declspec(noreturn) void ThrowErrnoError(int err)
{
#pragma warning(push)
#pragma warning(disable: 4996)
	throw errno_exception(err, strerror(err));
#pragma warning(pop)
}
