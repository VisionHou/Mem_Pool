#pragma once
#include"common.h"


class PageCache
{
public:
	static PageCache* GetInstance()
	{
		return &_inst;
	}

	//申请一个新的span
	Span* NewSpan(size_t npage);
	Span* _NewSpan(size_t npage);


	//获取对象到span的映射
	Span* MapObjectToSpan(void* obj);
	
	//释放空闲span到pagecache，合并相邻页
	void ReleaseSpanToPageCache(Span* span);

private:
	SpanList _pagelist[NPAGES];

private:
	PageCache() = default;
	PageCache(const PageCache&) = delete;
	PageCache& operator=(const PageCache&) = delete;
	static PageCache _inst;//全局只有唯一对象

	std::mutex _mtx;

	//std::unordered_map<PageID, Span*> _id_span_map;
	std::map<PageID, Span*> _id_span_map;
};