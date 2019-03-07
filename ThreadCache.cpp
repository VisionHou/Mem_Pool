#include"ThreadCache.h"
#include"CentralCache.h"

//从中心缓存批量化获取内存对象
void* ThreadCache::FetchFromCentralCache(size_t index, size_t byte)
{
	FreeList* freelist = &_freelist[index];
	size_t num_to_move = min(ClassSize::NumMoveSize(byte),freelist->MaxSize());
	void* start;
	void* end;
	size_t fetchnum = CentralCache::GetInstance()->FetchRangeObj(start,end, num_to_move,byte);
	if (fetchnum > 1)
		freelist->PushRange(NEXT_OBJ(start), end, fetchnum - 1);//?start下一个

	if (num_to_move == freelist->MaxSize())
	{
		freelist->SetMaxSize(num_to_move + 1);
	}
	return start;
}

//申请内存对象
void* ThreadCache::Allocate(size_t size)
{
	assert(size < MAXBYTES);

	//size向上对齐插入自由链表
	size = ClassSize::Roundup(size);
	size_t index = ClassSize::Index(size);
	FreeList* freelist = &_freelist[index];
	if (!freelist->Empty())
	{
		return freelist->Pop();
	}
	else
	{
		//?不用切分ThreadCache
		return FetchFromCentralCache(index, size);
	}
}

//释放内存对象
void ThreadCache::Deallocate(void* ptr, size_t byte)
{
	assert(byte < MAXBYTES);
	size_t index = ClassSize::Index(byte);//?不用对齐size
	FreeList* freelist = &_freelist[index];
	freelist->Push(ptr);

	//自由链表太长还给CentralCache
	if (freelist->Size() > freelist->MaxSize())
	{
		ListTooLong(freelist,byte);
	}

	//thread cache总的字节超出2M，就开始释放
}

void ThreadCache::ListTooLong(FreeList* freelist, size_t byte)
{
	void* start = freelist->Clear();
	CentralCache::GetInstance()->ReleaseListToSpans(start, byte);
}


