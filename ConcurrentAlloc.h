#pragma once
#include"ThreadCache.h"
#include"common.h"
#include"PageCache.h"


static void* ConcurrentAlloc(size_t size)
{
	if (size > MAXBYTES)
	{
		//return malloc(size);//?可以malloc(MAXBYTES=size)
		size_t roundsize = ClassSize::_Roundup(size,1<<PAGE_SHIFT);//2^10=1k 2^12=4k
		size_t npage = roundsize >> PAGE_SHIFT;//是多少页

		Span* span=PageCache::GetInstance()->NewSpan(npage);
		void* ptr = (void*)(span->_pageid << PAGE_SHIFT);

		return ptr;
	}
	else
	{
		//通过tls，获取线程自己的threadcache
		if (tls_threadcache == nullptr)
		{
			tls_threadcache = new ThreadCache;
			//cout << std::this_thread::get_id() << "->" << tls_threadcache << endl;
		}

		return tls_threadcache->Allocate(size);
	}
}


//释放需要指定大小，每次头上多开4个字节 会浪费内存
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