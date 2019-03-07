#include"CentralCache.h"
#include"PageCache.h"

CentralCache CentralCache::_inst;//定义

/*
//打桩->从中心缓存获取一定数量的对象给ThreadCache
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t n, size_t bytes)
{
	start = malloc(bytes*n);//开辟好一段空间
	end = (char*)start + (n - 1)*bytes;

	//小段空间链接起来
	void* cur = start;
	while (cur <= end)
	{
		void* next = (char*)cur + bytes;
		NEXT_OBJ(cur) = next;
		cur = next;
	}

	NEXT_OBJ(end) = nullptr;

	return n;
}
*/

//获取一个span对象
Span* CentralCache::GetOneSpan(SpanList* spanlist, size_t bytes)
{
	Span* span = spanlist->begin();
	while (span != spanlist->end())
	{
		if (span->_objlist != nullptr)
		{
			//对象链表一定有内存
			return span;
		}

		span = span->_next;
	}

	//向页缓存申请合适大小的span
	size_t npage = ClassSize::NumMovePage(bytes);
	Span* newspan = PageCache::GetInstance()->NewSpan(npage);

	// 将span的内存切割成一个个bytes大小的对象挂起来
	char* start = (char*)(newspan->_pageid << PAGE_SHIFT);
	char* end = start + (newspan->npage << PAGE_SHIFT);
	char* cur = start;
	char* next = cur + bytes;
	while (next < end)
	{
		NEXT_OBJ(cur) = next;
		cur = next;
		next = cur + bytes;
	}
	NEXT_OBJ(cur) = nullptr;
	newspan->_objlist = start;
	newspan->_objsize = bytes;
	newspan->_usecount = 0;

	// 将newspan插入到spanlist
	spanlist->PushFront(newspan);
	return newspan;
}

//从中心缓存获取一定数量的对象给ThreadCache
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t n, size_t bytes)
{
	size_t index = ClassSize::Index(bytes);
	SpanList* spanlist = &_spanlist[index];

	// 对当前桶进行加锁
	std::unique_lock<std::mutex> lock(spanlist->_mtx);

	Span* span = GetOneSpan(spanlist, bytes);

	void* cur = span->_objlist;
	void* prev = cur;
	size_t fetchnum = 0;
	while (fetchnum < n && !cur)
	{
		prev = cur;
		cur = NEXT_OBJ(cur);
		++fetchnum;
	}
	//获得一段对象内存
	start = span->_objlist;
	end = prev;
	NEXT_OBJ(end) = nullptr;

	//将span的对象链表再连上
	span->_objlist = cur;
	span->_usecount += fetchnum;
	

	//当一个对象为空，将span移到尾上？？？？
	if (span->_objlist == nullptr)
	{
		spanlist->Erase(span);
		spanlist->PushBack(span);
	}

	return fetchnum;
}


//将一定数量的对象释放到span跨度
void CentralCache::ReleaseListToSpans(void* start, size_t byte)
{
	size_t index = ClassSize::Index(byte);
	SpanList* spanlist = &_spanlist[index];

	std::unique_lock<std::mutex> lock(spanlist->_mtx);

	while (start)
	{
		void* next = NEXT_OBJ(start);
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);

		//当一个对象为空，将span移到头上？？？？？
		if (span->_objlist == nullptr)
		{
			spanlist->Erase(span);
			spanlist->PushFront(span);
	}

		//双向？？？？？
		NEXT_OBJ(start) = span->_objlist;
		span->_objlist = start;

		//使用计数为零说明span切出去的对象都还回来了
		//span释放到页缓存合并
		if (--span->_usecount == 0)
		{
			spanlist->Erase(span);
			span->_objlist = nullptr;
			span->_objsize = 0;
			span->_prev = nullptr;
			span->_next = nullptr;

			PageCache::GetInstance()->ReleaseSpanToPageCache(span);

		}
		start = next;
	}
}