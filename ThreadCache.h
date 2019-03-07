#pragma once

#include"common.h"

class ThreadCache
{
public:
	//�����ڴ����
	void* Allocate(size_t size);
	
	//�ͷ��ڴ����
	void Deallocate(void* ptr, size_t size);

	//�����Ļ����ȡ�ڴ����
	void* FetchFromCentralCache(size_t index, size_t bytes);

	//��������̫����ʼ�ͷŵ����Ļ���
	void ListTooLong(FreeList* freelist,size_t byte);
private:
	FreeList _freelist[NLISTS];
	//int tid;
	//ThreadCache* next;
};
//tls��֤�߳����Լ�������ȫ�ֱ���
static _declspec(thread) ThreadCache* tls_threadcache = nullptr;