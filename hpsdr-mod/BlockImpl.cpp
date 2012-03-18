#include "stdafx.h"
#include "BlockImpl.h"

#include "windows.h"

// ------------------------------------------------------------------ class CRefcountObject

unsigned CRefcountObject::AddRef()
{
	return InterlockedIncrement(&m_refCount);
}

unsigned CRefcountObject::Release()
{
	unsigned newref = InterlockedDecrement(&m_refCount);
	if(!newref) delete this;
	return newref;
}
