#pragma once

#include"common.h"

class ThreadCache
{
public:
	//申请内存对象
	void* Allocate(size_t size);
	
	//释放内存对象
	void Deallocate(void* ptr, size_t size);

	//从中心缓存获取内存对象
	void* FetchFromCentralCache(size_t index, size_t bytes);

	//自由链表太长开始释放到中心缓存
	void ListTooLong(FreeList* freelist,size_t byte);
private:
	FreeList _freelist[NLISTS];
	//int tid;
	//ThreadCache* next;
};
//tls保证线程有自己独立的全局变量
static _declspec(thread) ThreadCache* tls_threadcache = nullptr;