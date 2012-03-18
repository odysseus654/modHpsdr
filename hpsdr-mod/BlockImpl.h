#pragma once
#include "block.h"

class CRefcountObject
{
public:
	inline CRefcountObject():m_refCount(0) {}
	virtual ~CRefcountObject() {}
	virtual unsigned AddRef();
	virtual unsigned Release();

private:
	volatile long m_refCount;
};