#pragma once
#include"common.h"

//单例模式
class CentralCache
{
public:
	static CentralCache* GetInstance()
	{
		return &_inst;
	}
	
	//从中心缓存获取一定数量的对象给ThreadCache
	size_t FetchRangeObj(void*& start, void*& end, size_t n, size_t bytes);

	//获取一个span对象
	Span* GetOneSpan(SpanList* spanlist, size_t bytes);

	//将一定数量的对象释放到span跨度
	void ReleaseListToSpans(void* start, size_t byte_size);

private:
	//中心缓存自由链表
	SpanList _spanlist[NLISTS];
private:
	CentralCache() = default;
	CentralCache(const CentralCache&) = delete;
	CentralCache& operator=(const CentralCache&) = delete;

	static CentralCache _inst;
};