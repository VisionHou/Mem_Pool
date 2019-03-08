#pragma once
#include<iostream>
#include<thread>
#include<vector>
#include<map>
#include<mutex>

#ifdef _WIN32
#include<windows.h>
#endif

using std::cout;
using std::endl;

#include<assert.h>

//�����������������ĳ���
const size_t NLISTS = 240;
const size_t MAXBYTES = 64 * 1024;
const size_t PAGE_SHIFT = 12;
const size_t NPAGES = 129;


inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	//��Ҫ��ϵͳ�����ڴ�
	VirtualFree(ptr,0,MEM_RELEASE);
	if (ptr == nullptr)
	{
		throw std::bad_alloc();
	}

#else if 
	//brk muap
#endif//_win32

}

inline static void* SystemAlloc(size_t npage)
{
#ifdef _WIN32
	//��Ҫ��ϵͳ�����ڴ�
	void* ptr = VirtualAlloc(NULL, (NPAGES - 1) << PAGE_SHIFT, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (ptr == nullptr)
	{
		throw std::bad_alloc();
	}

#else if 
	//brk mmap
#endif//_win32

	return ptr;

}

static inline void*& NEXT_OBJ(void* obj)
{
	return *((void**)obj);
}

//���Ļ���ʵ�����ǹ�ϣӳ���Span������������
typedef size_t PageID;
//��ͷѭ��˫������,��ҳΪ��λ
struct Span
{
	PageID _pageid=0;//ҳ��:ҳ��*һҳ��С=������ʼ��ַ
	size_t npage=0;  //ҳ���������С

	Span* _next=nullptr;
	Span* _prev=nullptr;

	void* _objlist = nullptr;//������������
	size_t _objsize = 0;//�������
	size_t _usecount = 0;//ʹ�ü���
};

class SpanList
{
public:
	SpanList()
	{
		_head = new Span;
		_head->_next = _head;
		_head->_prev = _head;
	}

	Span* begin()
	{
		return _head->_next;
	}

	Span* end()
	{
		return _head->_prev;
	}

	bool Empty()
	{
		return _head->_next ==_head;
	}

	void Insert(Span* cur, Span* newspan)
	{
		assert(cur);
		Span* prev = cur->_prev;

		prev->_next = newspan;
		newspan->_prev = prev;
		newspan->_next = cur;
		cur->_prev = newspan;
	}

	void Erase(Span* cur)
	{
		assert(cur != nullptr&&cur != _head);
		Span* prev = cur->_prev;
		Span* next = cur->_next;

		prev->_next = next;
		next->_prev = prev;
	}


	void PushBack(Span* span)
	{
		Insert(end(), span);
	}



	Span*  PopBack(Span* span)
	{
		Span* tail= _head->_prev;
		Erase(tail);
		return tail;
	}

	void PushFront(Span* span)
	{
		Insert(begin(), span);
	}

	Span* PopFront()
	{
		Span* span = begin();
		Erase(span);
		return span;
	}

	std::mutex _mtx;
private:
	Span* _head = nullptr;
};

//ThreadCache����������
class FreeList
{
public:
	bool Empty()
	{
		return _list == nullptr;
	}

	void PushRange(void* start, void* end,size_t num)
	{
		NEXT_OBJ(end) = _list;
		_list = start;
		_size += num;
	}

	void* Clear()
	{
		_size = 0;
		void* list = _list;
		_list = nullptr;
		return list;
	}

	void* Pop()
	{
		void* obj = _list;
		_list = NEXT_OBJ(obj);
		--_size;

		return obj;
	}

	void Push(void* obj)
	{
		NEXT_OBJ(obj) = _list;
		_list = obj;
		++_size;
	}

	size_t Size()
	{
		return _size;
	}

	void SetMaxSize(size_t maxsize)
	{
		_maxsize = maxsize;
	}

	size_t MaxSize()
	{
		return _maxsize;
	}
private:
	void* _list = nullptr;
	size_t _size = 0;
	size_t _maxsize = 1;
};


//���ƹ��������С��ӳ�����
class ClassSize
{
public:
	// ������12%���ҵ�����Ƭ�˷�
    // [1,128] 8byte���� freelist[0,16)
    // [129,1024] 16byte���� freelist[16,72)
    // [1025,8*1024] 128byte���� freelist[72,128)
    // [8*1024+1,64*1024] 512byte���� freelist[128,240)

	static inline size_t _Roundup(size_t size, size_t align)
	{
		return (size + (align - 1))&~(align - 1);
	}

	//��ö�����size
	static inline size_t Roundup(size_t size)
	{
		assert(size < MAXBYTES);

		if (size <= 128)
		{
			return _Roundup(size, 8);
		}
		else if (size <= 1024)
		{
			return _Roundup(size, 16);
		}
		else if (size <= 8192)
		{
			return _Roundup(size, 128);
		}
		else if (size <= 65536)
		{
			return _Roundup(size, 512);
		}
		else
		{
			return -1;
		}
	}
	static inline size_t _Index(size_t bytes,size_t align_shift)//?Ϊʲô
	{
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
	}

	//�����������������е�λ��
	static inline size_t Index(size_t bytes)
	{
		assert(bytes < MAXBYTES);

		//ÿ���������������ĸ���
		static int group_array[4] = { 16,56,56,112 };

		if (bytes <= 128)
		{
			return _Index(bytes, 3);
		}
		else if (bytes <= 1024) {
			return _Index(bytes - 128, 4) + group_array[0];
		}
		else if (bytes <= 8192) {
			return _Index(bytes - 1024, 7) + group_array[1] + group_array[0];
		}
		else if (bytes <= 65536) {
			return _Index(bytes - 8192, 9) + group_array[2] + group_array[1] +
				group_array[0];
		}
		assert(false);
		return -1;
	}

	//����һ����ϵͳ��ȡ����ĸ���
	static size_t NumMoveSize(size_t size)
	{
		if (size == 0)
			return 0;

		int num = static_cast<int>(MAXBYTES / size);
		if (num < 2)
			num = 2;

		if (num > 52)
			num = 52;

		return num;
	}

	//����һ����ϵͳ��ȡҳ�ĸ���
	static size_t NumMovePage(size_t size)
	{
		size_t num = NumMoveSize(size);
		size_t npage = num*size;

		npage >>= 12;//����12����ҳ����
		if (npage == 0)
			return 1;

		return npage;
	}
};