#pragma once
#include"common.h"


class PageCache
{
public:
	static PageCache* GetInstance()
	{
		return &_inst;
	}

	//����һ���µ�span
	Span* NewSpan(size_t npage);
	Span* _NewSpan(size_t npage);


	//��ȡ����span��ӳ��
	Span* MapObjectToSpan(void* obj);
	
	//�ͷſ���span��pagecache���ϲ�����ҳ
	void ReleaseSpanToPageCache(Span* span);

private:
	SpanList _pagelist[NPAGES];

private:
	PageCache() = default;
	PageCache(const PageCache&) = delete;
	PageCache& operator=(const PageCache&) = delete;
	static PageCache _inst;//ȫ��ֻ��Ψһ����

	std::mutex _mtx;

	//std::unordered_map<PageID, Span*> _id_span_map;
	std::map<PageID, Span*> _id_span_map;
};