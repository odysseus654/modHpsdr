#pragma once
// derived from comip.h

template<typename _InterfaceType, typename _CommonType = IUnknown> class unk_ref_t
{
public:
	typedef _InterfaceType base_type;
	typedef _CommonType common_type;
	
	// Default constructor.
	inline unk_ref_t() throw()
		: m_pInterface(NULL)
	{
	}
	
	// Copy the pointer and AddRef().
	inline unk_ref_t(const unk_ref_t& cp) throw()
		: m_pInterface(cp.m_pInterface)
	{
		_AddRef(); 
	}
	
	// Saves the interface.
	inline explicit unk_ref_t(_InterfaceType* pInterface) throw()
		: m_pInterface(pInterface)
	{
		_AddRef(); 
	}
	
	// Copies the pointer. If fAddRef is TRUE, the interface will be AddRef()ed.
	inline unk_ref_t(_InterfaceType* pInterface, bool fAddRef) throw()
		: m_pInterface(pInterface)
	{
		if (fAddRef) _AddRef();
	}
	
	// Saves the interface.
	unk_ref_t& operator=(_InterfaceType* pInterface) throw()
	{
		if (m_pInterface != pInterface)
		{
			_InterfaceType* pOldInterface = m_pInterface;
			m_pInterface = pInterface;
			_AddRef();
			if (pOldInterface != NULL) pOldInterface->Release();
		}
		return *this;
	}
	
	// Copies and AddRef()'s the interface.
	inline unk_ref_t& operator=(const unk_ref_t& cp) throw()
	{
		return operator=(cp.m_pInterface); 
	}
	
	// If we still have an interface then Release() it. The interface
	// may be NULL if Detach() has previously been called, or if it was
	// never set.
	~unk_ref_t() throw()
	{
		_Release(); 
	}
	
	// Saves/sets the interface without AddRef()ing. This call
	// will release any previously acquired interface.
	void Attach(_InterfaceType* pInterface) throw()
	{
		_Release();
		m_pInterface = pInterface;
	}
	
	// Saves/sets the interface only AddRef()ing if fAddRef is TRUE.
	// This call will release any previously acquired interface.
	void Attach(_InterfaceType* pInterface, bool fAddRef) throw()
	{
		_Release();
		m_pInterface = pInterface;
		if (fAddRef && pInterface != NULL)
		{
			pInterface->AddRef();
		}
	}
	
	// Simply NULL the interface pointer so that it isn't Released()'ed.
	_InterfaceType* Detach() throw()
	{
		Interface* const old = (_InterfaceType*)m_pInterface;
		m_pInterface = NULL;
		return old;
	}
	
	// Return the interface. This value may be NULL.
	inline operator _InterfaceType*() const throw()
	{
		return (_InterfaceType*)m_pInterface; 
	}
	
	// Queries for the unknown and return it Provides minimal level error checking before use.
	operator _InterfaceType&() const 
	{
		ASSERT(m_pInterface != NULL);
		return *(_InterfaceType*)m_pInterface; 
	}
	
	// Allows an instance of this class to act as though it were the
	// actual interface. Also provides minimal error checking.
	_InterfaceType& operator*() const 
	{
		ASSERT(m_pInterface != NULL);
		return *(_InterfaceType*)m_pInterface; 
	}
	/*
	// Returns the address of the interface pointer contained in this
	// class. This is useful when using the COM/OLE interfaces to create
	// this interface.
	_InterfaceType** operator&() throw()
	{
		_Release();
		m_pInterface = NULL;
		return &(_InterfaceType*)m_pInterface;
	}
	*/

	inline _InterfaceType** ref() const throw()
	{
		return (_InterfaceType**)&m_pInterface;
	}

	// Returns the address of the interface pointer contained in this
	// class. This is useful when using the COM/OLE interfaces to create
	// this interface.
	_InterfaceType** inref() throw()
	{
		_Release();
		m_pInterface = NULL;
		return (_InterfaceType**)&m_pInterface;
	}

	// Allows this class to be used as the interface itself. Also provides simple error checking.
	_InterfaceType* operator->() const 
	{
		ASSERT(m_pInterface != NULL);
		return (_InterfaceType*)m_pInterface; 
	}
	
	// This operator is provided so that simple boolean expressions will
	// work.  For example: "if (p) ...".
	// Returns TRUE if the pointer is not NULL.
	inline operator bool() const throw()
	{
		return m_pInterface != NULL; 
	}
	
	// Compare two smart pointers
	inline bool operator==(const unk_ref_t& p) const
	{
		return p.m_pInterface == m_pInterface;
	}
	
	// Compare two pointers
	inline bool operator==(_InterfaceType* p) const
	{
		return p == m_pInterface;
	}
	
	// Compare two smart pointers
	inline bool operator!=(const unk_ref_t& p) const
	{
		return p.m_pInterface != m_pInterface;
	}
	
	// Compare two pointers
	inline bool operator!=(_InterfaceType* p) const
	{
		return p != m_pInterface;
	}
	
	// Compare two smart pointers
	inline bool operator<(const unk_ref_t& p) const
	{
		return p.m_pInterface < m_pInterface;
	}
	
	// Compare two pointers
	inline bool operator<(_InterfaceType* p) const
	{
		return p < m_pInterface;
	}
	
	// Compare two smart pointers
	inline bool operator>(const unk_ref_t& p) const
	{
		return p.m_pInterface > m_pInterface;
	}
	
	// Compare two pointers
	inline bool operator>(_InterfaceType* p) const
	{
		return p > m_pInterface;
	}
	
	// Compare two smart pointers
	inline bool operator<=(const unk_ref_t& p) const
	{
		return p.m_pInterface <= m_pInterface;
	}
	
	// Compare two pointers
	inline bool operator<=(_InterfaceType* p) const
	{
		return p <= m_pInterface;
	}
	
	// Compare two smart pointers
	inline bool operator>=(const unk_ref_t& p) const
	{
		return p.m_pInterface >= m_pInterface;
	}
	
	// Compare two pointers
	inline bool operator>=(_InterfaceType* p) const
	{
		return p >= m_pInterface;
	}
	
	// Provides error-checking Release()ing of this interface.
	void Release() 
	{
		if (m_pInterface != NULL)
		{
			m_pInterface->Release();
			m_pInterface = NULL;
		}
	}
	
	// Provides error-checking AddRef()ing of this interface.
	void AddRef() 
	{
		if (m_pInterface != NULL)
		{
			m_pInterface->AddRef();
		}
	}
	
	// Another way to get the interface pointer without casting.
	inline _InterfaceType* GetInterfacePtr() const throw()
	{
		return (_InterfaceType*)m_pInterface; 
	}
	
	// Another way to get the interface pointer without casting
	// Use for [in, out] parameter passing
	_InterfaceType*& GetInterfacePtr() throw()
	{
		return (_InterfaceType*)m_pInterface; 
	}
	
private:
	// The Interface.
	_CommonType* m_pInterface;
	
	// Releases only if the interface is not null.
	// The interface is not set to NULL.
	void _Release() throw()
	{
		if (m_pInterface != NULL)
		{
			m_pInterface->Release();
		}
	}
	
	// AddRefs only if the interface is not NULL
	void _AddRef() throw()
	{
		if (m_pInterface != NULL)
		{
			m_pInterface->AddRef();
		}
	}
};

// Reverse comparison operators for _com_ptr_t
template<typename _OtherType, typename _OtherCommon>
inline bool operator==(_OtherType* i, const unk_ref_t<_OtherType,_OtherCommon>& p) 
{
    return p == i;
}

template<typename _OtherType, typename _OtherCommon>
inline bool operator!=(_OtherType* i, const unk_ref_t<_OtherType,_OtherCommon>& p) 
{
    return p != i;
}

template<typename _OtherType, typename _OtherCommon>
inline bool operator<(_OtherType* i, const unk_ref_t<_OtherType,_OtherCommon>& p) 
{
    return p > i;
}

template<typename _OtherType, typename _OtherCommon>
inline bool operator>(_OtherType* i, const unk_ref_t<_OtherType,_OtherCommon>& p) 
{
    return p < i;
}

template<typename _OtherType, typename _OtherCommon>
inline bool operator<=(_OtherType* i, const unk_ref_t<_OtherType,_OtherCommon>& p) 
{
    return p >= i;
}

template<typename _OtherType, typename _OtherCommon>
inline bool operator>=(_OtherType* i, const unk_ref_t<_OtherType,_OtherCommon>& p) 
{
    return p <= i;
}
