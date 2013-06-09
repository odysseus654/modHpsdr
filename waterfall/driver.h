#pragma once
#include <blockImpl.h>

template<class OBJ>
class CDirectxDriver : public signals::IBlockDriver
{
public:
	inline CDirectxDriver() {}
	virtual ~CDirectxDriver() {}

private:
	CDirectxDriver(const CDirectxDriver& other);
	CDirectxDriver operator=(const CDirectxDriver& other);

public:
	virtual const char* Name()			{ return NAME; }
	virtual const char* Description()	{ return DESCR; }
	virtual BOOL canCreate()			{ return true; }
	virtual BOOL canDiscover()			{ return false; }
	virtual unsigned Discover(signals::IBlock** blocks, unsigned availBlocks) { return 0; }
	virtual const unsigned char* Fingerprint() { return FINGERPRINT; }

	virtual signals::IBlock* Create()
	{
		signals::IBlock* blk = new OBJ(this);
		blk->AddRef();
		return blk;
	}

protected:
	static const char* NAME;
	static const char* DESCR;
	static const unsigned char FINGERPRINT[];
};
template<class OBJ> const unsigned char CDirectxDriver<OBJ>::FINGERPRINT[] = { 1, signals::etypVecDouble, 0 };
