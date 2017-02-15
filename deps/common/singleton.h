#ifndef SINGLETON_T_H
#define SINGLETON_T_H
#pragma once

#ifndef assert
#include <assert.h>
#endif

template <typename T,bool delay_create=true>
class CSingleTon_t
{
protected:
	CSingleTon_t()
	{
	}
	~CSingleTon_t()
	{
	}

public:
	static T* GetInstance()
	{
		static T _instance;
		return &_instance;
	}
};

template <typename T>
class CSingleTon_t<T,false>
{
protected:
	static T* m_instance;
public:
	CSingleTon_t()
	{
		assert(m_instance==NULL);
		m_instance = (T*)this;
	}
	~CSingleTon_t()
	{
		assert(m_instance==this);
		m_instance = NULL;
	}
	static T* GetInstance()
	{
		return m_instance;
	}
};

template <typename T> T* CSingleTon_t<T,false>::m_instance=NULL;

/*
class AObject:
	public CSingleTon_t<AObject>
{
public:
	void Op()
	{

	}
};

int _tmain(int argc, _TCHAR* argv[])
{
	AObject::GetInstance()->Op();

	return 0;
}
*/

#endif
