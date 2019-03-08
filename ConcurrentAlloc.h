#pragma once
#include"ThreadCache.h"
#include"common.h"
#include"PageCache.h"


static void* ConcurrentAlloc(size_t size)
{
	if (size > MAXBYTES)
	{
		//return malloc(size);//?����malloc(MAXBYTES=size)
		size_t roundsize = ClassSize::_Roundup(size,1<<PAGE_SHIFT);//2^10=1k 2^12=4k
		size_t npage = roundsize >> PAGE_SHIFT;//�Ƕ���ҳ

		Span* span=PageCache::GetInstance()->NewSpan(npage);
		void* ptr = (void*)(span->_pageid << PAGE_SHIFT);

		return ptr;
	}
	else
	{
		//ͨ��tls����ȡ�߳��Լ���threadcache
		if (tls_threadcache == nullptr)
		{
			tls_threadcache = new ThreadCache;
			//cout << std::this_thread::get_id() << "->" << tls_threadcache << endl;
		}

		return tls_threadcache->Allocate(size);
	}
}


//�ͷ���Ҫָ����С��ÿ��ͷ�϶࿪4���ֽ� ���˷��ڴ�
//
static void ConcurrentFree(void* ptr)
{
	Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
	size_t size = span->_objsize;
	if (size > MAXBYTES)
	{
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
	}
	else
	{
		tls_threadcache->Deallocate(ptr, size);
	}
}