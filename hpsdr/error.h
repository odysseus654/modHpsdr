#pragma once
#include <stdexcept>

class socket_exception : public std::runtime_error
{
public:
	int code;
	inline socket_exception(int err, const char* str)
		:std::runtime_error(str),code(err)
	{}
	inline socket_exception(int err)
		:std::runtime_error(""),code(err)
	{}
};

class lasterr_exception : public std::runtime_error
{
public:
	DWORD code;
	inline lasterr_exception(int err, const char* str)
		:std::runtime_error(str),code(err)
	{}
	inline lasterr_exception(int err)
		:std::runtime_error(""),code(err)
	{}
};

class errno_exception : public std::runtime_error
{
public:
	int code;
	inline errno_exception(int err, const char* str)
		:std::runtime_error(str),code(err)
	{}
	inline errno_exception(int err)
		:std::runtime_error(""),code(err)
	{}
};

__declspec(noreturn) void ThrowSocketError(int err);
__declspec(noreturn) void ThrowLastError(DWORD err);
__declspec(noreturn) void ThrowErrnoError(int err);
