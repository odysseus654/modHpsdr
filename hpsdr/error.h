/*
	Copyright 2012 Erik Anderson

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
