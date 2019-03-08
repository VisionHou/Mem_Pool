#include"PageCache.h"

PageCache PageCache::_inst;


Span* PageCache::NewSpan(size_t npage)
{
	std::unique_lock<std::mutex> lock(_mtx);
	if (npage >= NPAGES)
	{
		//��ϵͳ���� ���ҽ�ȥ ӳ��span
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
	//std::unique_lock<std::mutex> lock(_mtx);//���Լ�������

	if (!_pagelist[npage].Empty())
	{
		return _pagelist[npage].PopFront();
	}

	for (size_t i = npage + 1; i < NPAGES; i++)
	{
		SpanList* pagelist = &_pagelist[i];
		if (!pagelist->Empty())
		{
			//����ȡ��span�ָ��һ����С��span
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

	//��Ҫ��ϵͳ�����ڴ�
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


//��ȡ����span��ӳ��
Span* PageCache::MapObjectToSpan(void* obj)
{
	PageID pageid = (PageID)obj >> PAGE_SHIFT;
	auto it = _id_span_map.find(pageid);
	assert(it != _id_span_map.end());

	return it->second;
}

//�ͷſ���span��pagecache���ϲ�����ҳ
void PageCache::ReleaseSpanToPageCache(Span* span)
{
	//����64k������
	std::unique_lock<std::mutex> lock(_mtx);//�п��ܶ��߳�
	if (span->npage >= NPAGES)
	{
		//ֱ���ͷŸ�ϵͳ
		void* ptr = (void*)(span->_pageid << PAGE_SHIFT);
		_id_span_map.erase(span->_pageid);
		SystemFree(ptr);
		delete span;
		return;
	}

	//��ǰ�ϲ�
	auto previt = _id_span_map.find(span->_pageid - 1);
	while (previt != _id_span_map.end())
	{
		Span* prevspan = previt->second;

		//���ǿ��У�����ѭ��
		if (prevspan->_usecount != 0)
			break;

		//�ϲ�ҳ���������ҳ���ϲ�
		if (prevspan->npage + span->npage >= NPAGES)
			break;

		//���ҳ��ǰ�ϲ�
		_pagelist[prevspan->npage].Erase(prevspan);
		prevspan->npage += span->npage;
		delete span;
		span = prevspan;

		previt = _id_span_map.find(span->_pageid - 1);
	}

	//����ҳ�ϲ�
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

	//���ϲ���Ĵ�ҳӳ�䵽map��
	for (size_t i = 0; i < span->npage; i++)
	{
		_id_span_map[span->_pageid + i] = span;
	}

	_pagelist[span->npage].PushFront(span);
}

