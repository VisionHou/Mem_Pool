#pragma once
#include"common.h"

//����ģʽ
class CentralCache
{
public:
	static CentralCache* GetInstance()
	{
		return &_inst;
	}
	
	//�����Ļ����ȡһ�������Ķ����ThreadCache
	size_t FetchRangeObj(void*& start, void*& end, size_t n, size_t bytes);

	//��ȡһ��span����
	Span* GetOneSpan(SpanList* spanlist, size_t bytes);

	//��һ�������Ķ����ͷŵ�span���
	void ReleaseListToSpans(void* start, size_t byte_size);

private:
	//���Ļ�����������
	SpanList _spanlist[NLISTS];
private:
	CentralCache() = default;
	CentralCache(const CentralCache&) = delete;
	CentralCache& operator=(const CentralCache&) = delete;

	static CentralCache _inst;
};