#include"CentralCache.h"
#include"PageCache.h"

CentralCache CentralCache::_inst;//����

/*
//��׮->�����Ļ����ȡһ�������Ķ����ThreadCache
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t n, size_t bytes)
{
	start = malloc(bytes*n);//���ٺ�һ�οռ�
	end = (char*)start + (n - 1)*bytes;

	//С�οռ���������
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

//��ȡһ��span����
Span* CentralCache::GetOneSpan(SpanList* spanlist, size_t bytes)
{
	Span* span = spanlist->begin();
	while (span != spanlist->end())
	{
		if (span->_objlist != nullptr)
		{
			//��������һ�����ڴ�
			return span;
		}

		span = span->_next;
	}

	//��ҳ����������ʴ�С��span
	size_t npage = ClassSize::NumMovePage(bytes);
	Span* newspan = PageCache::GetInstance()->NewSpan(npage);

	// ��span���ڴ��и��һ����bytes��С�Ķ��������
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

	// ��newspan���뵽spanlist
	spanlist->PushFront(newspan);
	return newspan;
}

//�����Ļ����ȡһ�������Ķ����ThreadCache
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t n, size_t bytes)
{
	size_t index = ClassSize::Index(bytes);
	SpanList* spanlist = &_spanlist[index];

	// �Ե�ǰͰ���м���
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
	//���һ�ζ����ڴ�
	start = span->_objlist;
	end = prev;
	NEXT_OBJ(end) = nullptr;

	//��span�Ķ�������������
	span->_objlist = cur;
	span->_usecount += fetchnum;
	

	//��һ������Ϊ�գ���span�Ƶ�β�ϣ�������
	if (span->_objlist == nullptr)
	{
		spanlist->Erase(span);
		spanlist->PushBack(span);
	}

	return fetchnum;
}


//��һ�������Ķ����ͷŵ�span���
void CentralCache::ReleaseListToSpans(void* start, size_t byte)
{
	size_t index = ClassSize::Index(byte);
	SpanList* spanlist = &_spanlist[index];

	std::unique_lock<std::mutex> lock(spanlist->_mtx);

	while (start)
	{
		void* next = NEXT_OBJ(start);
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);

		//��һ������Ϊ�գ���span�Ƶ�ͷ�ϣ���������
		if (span->_objlist == nullptr)
		{
			spanlist->Erase(span);
			spanlist->PushFront(span);
	}

		//˫�򣿣�������
		NEXT_OBJ(start) = span->_objlist;
		span->_objlist = start;

		//ʹ�ü���Ϊ��˵��span�г�ȥ�Ķ��󶼻�������
		//span�ͷŵ�ҳ����ϲ�
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