#include"ThreadCache.h"
#include"CentralCache.h"

//�����Ļ�����������ȡ�ڴ����
void* ThreadCache::FetchFromCentralCache(size_t index, size_t byte)
{
	FreeList* freelist = &_freelist[index];
	size_t num_to_move = min(ClassSize::NumMoveSize(byte),freelist->MaxSize());
	void* start;
	void* end;
	size_t fetchnum = CentralCache::GetInstance()->FetchRangeObj(start,end, num_to_move,byte);
	if (fetchnum > 1)
		freelist->PushRange(NEXT_OBJ(start), end, fetchnum - 1);//?start��һ��

	if (num_to_move == freelist->MaxSize())
	{
		freelist->SetMaxSize(num_to_move + 1);
	}
	return start;
}

//�����ڴ����
void* ThreadCache::Allocate(size_t size)
{
	assert(size < MAXBYTES);

	//size���϶��������������
	size = ClassSize::Roundup(size);
	size_t index = ClassSize::Index(size);
	FreeList* freelist = &_freelist[index];
	if (!freelist->Empty())
	{
		return freelist->Pop();
	}
	else
	{
		//?�����з�ThreadCache
		return FetchFromCentralCache(index, size);
	}
}

//�ͷ��ڴ����
void ThreadCache::Deallocate(void* ptr, size_t byte)
{
	assert(byte < MAXBYTES);
	size_t index = ClassSize::Index(byte);//?���ö���size
	FreeList* freelist = &_freelist[index];
	freelist->Push(ptr);

	//��������̫������CentralCache
	if (freelist->Size() > freelist->MaxSize())
	{
		ListTooLong(freelist,byte);
	}

	//thread cache�ܵ��ֽڳ���2M���Ϳ�ʼ�ͷ�
}

void ThreadCache::ListTooLong(FreeList* freelist, size_t byte)
{
	void* start = freelist->Clear();
	CentralCache::GetInstance()->ReleaseListToSpans(start, byte);
}


