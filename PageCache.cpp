#include"PageCache.h"

PageCache PageCache::_inst;


Span* PageCache::NewSpan(size_t npage)
{
	std::unique_lock<std::mutex> lock(_mtx);
	if (npage >= NPAGES)
	{
		//向系统申请 不挂进去 映射span
		void* ptr = SystemAlloc(npage);
		Span* span = new Span;
		span->_pageid = (PageID)ptr >> PAGE_SHIFT;
		span->npage = npage;
		span->_objsize = npage << PAGE_SHIFT;

		_id_span_map[span->_pageid] = span;
		
		return span;
	}
	Span* span= _NewSpan(npage);
	span->_objsize = npage << PAGE_SHIFT;
	return span;
}
Span* PageCache::_NewSpan(size_t npage)
{
	//std::unique_lock<std::mutex> lock(_mtx);//调自己会死锁

	if (!_pagelist[npage].Empty())
	{
		return _pagelist[npage].PopFront();
	}

	for (size_t i = npage + 1; i < NPAGES; i++)
	{
		SpanList* pagelist = &_pagelist[i];
		if (!pagelist->Empty())
		{
			//将获取的span分割成一个个小的span
			Span* span = pagelist->PopFront();
			Span* split = new Span;
			split->_pageid = span->_pageid + span->npage - npage;
			split->npage = npage;
			span->npage -= npage;

			_pagelist[span->npage].PushFront(span);

			for (size_t i = 0; i < split->npage; i++)
			{
				_id_span_map[split->_pageid + i] = split;
			}

			return split;
		}
	}

	//需要向系统申请内存
	void* ptr = SystemAlloc(npage);

	Span* largespan = new Span;
	largespan->_pageid = (PageID)ptr >> PAGE_SHIFT;
	largespan->npage = NPAGES - 1;

	_pagelist[NPAGES - 1].PushFront(largespan);

	for (size_t i = 0; i < largespan->npage; i++)
	{
		_id_span_map[largespan->_pageid + i] = largespan;
	}

	return _NewSpan(npage);
}


//获取对象到span的映射
Span* PageCache::MapObjectToSpan(void* obj)
{
	PageID pageid = (PageID)obj >> PAGE_SHIFT;
	auto it = _id_span_map.find(pageid);
	assert(it != _id_span_map.end());

	return it->second;
}

//释放空闲span到pagecache，合并相邻页
void PageCache::ReleaseSpanToPageCache(Span* span)
{
	//大于64k回来的
	std::unique_lock<std::mutex> lock(_mtx);//有可能多线程
	if (span->npage >= NPAGES)
	{
		//直接释放给系统
		void* ptr = (void*)(span->_pageid << PAGE_SHIFT);
		_id_span_map.erase(span->_pageid);
		SystemFree(ptr);
		delete span;
		return;
	}

	//向前合并
	auto previt = _id_span_map.find(span->_pageid - 1);
	while (previt != _id_span_map.end())
	{
		Span* prevspan = previt->second;

		//不是空闲，跳出循环
		if (prevspan->_usecount != 0)
			break;

		//合并页数大于最大页不合并
		if (prevspan->npage + span->npage >= NPAGES)
			break;

		//后边页向前合并
		_pagelist[prevspan->npage].Erase(prevspan);
		prevspan->npage += span->npage;
		delete span;
		span = prevspan;

		previt = _id_span_map.find(span->_pageid - 1);
	}

	//向后边页合并
	auto nextit = _id_span_map.find(span->_pageid + span->npage);
	while (nextit != _id_span_map.end())
	{
		Span* nextspan = nextit->second;
		
		if (nextspan->_usecount != 0)
			break;
		if (nextspan->npage + span->npage > NPAGES)
			break;

		_pagelist[nextspan->npage].Erase(nextspan);
		span->npage += nextspan->npage;
		delete nextspan;

		nextit = _id_span_map.find(span->_pageid + span->npage);
	}

	//将合并完的大页映射到map中
	for (size_t i = 0; i < span->npage; i++)
	{
		_id_span_map[span->_pageid + i] = span;
	}

	_pagelist[span->npage].PushFront(span);
}

