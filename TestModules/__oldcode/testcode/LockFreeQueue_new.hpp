#ifndef __LOCKFREE_QUEUE_NEW_H__
#define __LOCKFREE_QUEUE_NEW_H__
#include "TlsObjectPool_IntrusiveList.hpp"

/////////////////////////////////////////////////////////////////////
// 1.기존의 비효율적 tail이동 제거 버전
/////////////////////////////////////////////////////////////////////

//#define USE_ALIGN
#ifdef USE_ALIGN
#define ALIGN64						alignas(64)
#else
#define ALIGN64
#endif

// 카운터로 사용할 비트 수 
#define LOCKFREE_QUEUE_COUNTER_BIT	17
// 시프트 할 횟수
#define LOCKFREE_QUEUE_SHIFT_BIT	47
// 마스크
#define LOCKFREE_QUEUE_BIT_MASK		0x00007FFF'FFFFFFFF
// 고유 풀 번호
#define LOCKFREE_QUEUE_POOL_NUM		0xFE00'0000


namespace Core
{
	/////////////////////////////////////////////////////////////////////
	// 미친듯이 생성삭제하면 _pMyNull(nullptr대용)이 겹칠 수 있으니 지양
	/////////////////////////////////////////////////////////////////////
	template <typename T>
	class CLockFreeQueue
	{
	public:
		enum QUEUE
		{
			MAX_QUEUE_SIZE = 1000
		};
		//----------------------------------------------------------------
		// 노드 구조체
		//----------------------------------------------------------------
		struct stNode
		{
			stNode* next;
			T data;
		};

		//----------------------------------------------------------------
		// Info: 인큐
		// Parameter: const T& data (넣을 데이터)
		// Return: 성공 true, 실패 false,  실패사유: size 오버
		//----------------------------------------------------------------
		bool Enqueue(const T& data)
		{
			if (_size > _maxSize)
				return false;

			stNode* newNode = s_nodePool.Alloc();
			newNode->next = _pMyNull;
			newNode->data = data;

			while (1)
			{
				stNode* tail = _tail;
				uint64_t counter = ((uint64_t)tail >> LOCKFREE_QUEUE_SHIFT_BIT) + 1;
				stNode* nextTail = (stNode*)((uint64_t)newNode | (counter << LOCKFREE_QUEUE_SHIFT_BIT));
				stNode* next = ((stNode*)((uint64_t)tail & LOCKFREE_QUEUE_BIT_MASK))->next;

				if (_InterlockedCompareExchangePointer((volatile PVOID*)&(((stNode*)((uint64_t)tail & LOCKFREE_QUEUE_BIT_MASK))->next), (void*)newNode, (void*)_pMyNull) == _pMyNull)
				{
					_InterlockedIncrement(&_size);
					_InterlockedCompareExchangePointer((volatile PVOID*)&_tail, (void*)nextTail, (void*)tail);
					break;
				}
				else
				{	//무한루프 방지, Tail 옮겨주기
					stNode* tail_next = ((stNode*)((uint64_t)tail & LOCKFREE_QUEUE_BIT_MASK))->next;
					nextTail = (stNode*)((uint64_t)tail_next | (counter << LOCKFREE_QUEUE_SHIFT_BIT));
					if (tail_next != _pMyNull)
					{
						_InterlockedCompareExchangePointer((volatile PVOID*)&_tail, (void*)nextTail, (void*)tail);
					}
				}


			}

			return true;
		}

		//----------------------------------------------------------------
		// Info: 인큐
		// Parameter: const T& data (넣을 데이터)
		// ** 무조건 성공함 **
		//----------------------------------------------------------------
		void Enqueue_NotFail(const T& data)
		{
			stNode* newNode = s_nodePool.Alloc();
			newNode->next = _pMyNull;
			newNode->data = data;

			while (1)
			{
				stNode* tail = _tail;
				uint64_t counter = ((uint64_t)tail >> LOCKFREE_QUEUE_SHIFT_BIT) + 1;
				stNode* nextTail = (stNode*)((uint64_t)newNode | (counter << LOCKFREE_QUEUE_SHIFT_BIT));
				stNode* next = ((stNode*)((uint64_t)tail & LOCKFREE_QUEUE_BIT_MASK))->next;

				if (_InterlockedCompareExchangePointer((volatile PVOID*)&(((stNode*)((uint64_t)tail & LOCKFREE_QUEUE_BIT_MASK))->next), (void*)newNode, (void*)_pMyNull) == _pMyNull)
				{
					_InterlockedIncrement(&_size);
					_InterlockedCompareExchangePointer((volatile PVOID*)&_tail, (void*)nextTail, (void*)tail);
					break;
				}
				else
				{	//무한루프 방지, Tail 옮겨주기
					stNode* tail_next = ((stNode*)((uint64_t)tail & LOCKFREE_QUEUE_BIT_MASK))->next;
					nextTail = (stNode*)((uint64_t)tail_next | (counter << LOCKFREE_QUEUE_SHIFT_BIT));
					if (tail_next != _pMyNull)
					{
						_InterlockedCompareExchangePointer((volatile PVOID*)&_tail, (void*)nextTail, (void*)tail);
					}
				}

			}
		}

		//----------------------------------------------------------------
		// Info: 디큐
		// Parameter: T& data (데이터 받을 메모리)
		// Return: 성공시 true, 실패시 false, 실패는 큐가 비어서 -> 경쟁에서 져서 누가 채감
		//----------------------------------------------------------------
		bool Dequeue(T& data)
		{
			stNode* ret;
			stNode* head;
			stNode* ret_next;
			stNode* next;
			while (1)
			{
				head = _head;
				uint64_t counter = ((uint64_t)head >> LOCKFREE_QUEUE_SHIFT_BIT) + 1;
				ret = (stNode*)((uint64_t)head & LOCKFREE_QUEUE_BIT_MASK);
				ret_next = ret->next;
				//----------------------------------------------------------
				// [1] head를 읽고 자다깻는데 재사용되서 next가 null
				//     (*** 혹은 다른 큐에서 재사용 될 수 있음)
				// [2] 진짜로 큐가 비어서
				//----------------------------------------------------------
				if (((uint64_t)ret_next & (~LOCKFREE_QUEUE_BIT_MASK)) == (~LOCKFREE_QUEUE_BIT_MASK))
				{
					if (head != _head)
						continue;	// [1]번상황
					return false;	// [2]번상황
				}
				next = (stNode*)((uint64_t)ret_next | (counter << LOCKFREE_QUEUE_SHIFT_BIT));

				//----------------------------------------------------------
				// 더미노드만 있는 경우
				//----------------------------------------------------------
				if (ret == (stNode*)((uint64_t)_tail & LOCKFREE_QUEUE_BIT_MASK))
				{
					if (((uint64_t)ret->next & (~LOCKFREE_QUEUE_BIT_MASK)) == (~LOCKFREE_QUEUE_BIT_MASK))
					{
						return false;	//진짜 빈거
					}
					else
					{
						stNode* tail = _tail;
						uint64_t tailcounter = ((uint64_t)tail >> LOCKFREE_QUEUE_SHIFT_BIT) + 1;
						stNode* tail_next = ((stNode*)((uint64_t)tail & LOCKFREE_QUEUE_BIT_MASK))->next;
						stNode* nextTail = (stNode*)((uint64_t)tail_next | (tailcounter << LOCKFREE_QUEUE_SHIFT_BIT));
						if (tail_next != _pMyNull)
						{
							_InterlockedCompareExchangePointer((void* volatile*)&_tail, (void*)nextTail, (void*)tail);
						}
						continue;
					}
				}

				T tempData = ret_next->data;

				if (_InterlockedCompareExchangePointer((void* volatile*)&_head, (void*)next, (void*)head) == head)
				{
					data = tempData;
					_InterlockedDecrement(&_size);
					break;
				}
			}

			s_nodePool.Free(ret);

			return true;
		}

		//----------------------------------------------------------------
		// 사이즈 얻기, 참고용이다
		//----------------------------------------------------------------
		int GetSize()
		{
			return (int)_size;
		}

		//----------------------------------------------------------------
		// 청소 -> 문제있네... 리스트 같은거 줘야하나..?
		//----------------------------------------------------------------
		void Clear()
		{
			while (1)
			{
				T data;
				if (Dequeue(data) == false)
					break;
			}
		}
		//----------------------------------------------------------------
		// 생성자
		//----------------------------------------------------------------
		CLockFreeQueue(int maxSize = MAX_QUEUE_SIZE) :_maxSize(maxSize)
		{
			//------------------------------------------------------------
			// 64비트가 아니면 생성을 막자
			//------------------------------------------------------------
			SYSTEM_INFO sysinfo;
			GetSystemInfo(&sysinfo);
			if (sysinfo.lpMaximumApplicationAddress != (LPVOID)0x00007FFF'FFFEFFFF)
			{
				__debugbreak();
			}
			//------------------------------------------------------------
			// V4: 나의 nullptr을 만들자
			// . 접근 불가 주소 (커널영역) _pMyNull을 nullptr대신 씀
			//------------------------------------------------------------
			unsigned long myCounter = _InterlockedIncrement(&CLockFreeQueue<T>::s_counter);
			_pMyNull = (stNode*)((~LOCKFREE_QUEUE_BIT_MASK) | (uint64_t)myCounter);
			//------------------------------------------------------------
			// 시작 준비는 head, tail이 더미 노드를 가리키도록
			//------------------------------------------------------------
			stNode* dummy = s_nodePool.Alloc();
			dummy->next = _pMyNull;
			_head = dummy;
			_tail = dummy;
			_size = 0;
		}
		//----------------------------------------------------------------
		// 소멸자
		//----------------------------------------------------------------
		~CLockFreeQueue()
		{
			Clear();
		}
		//----------------------------------------------------------------
		// 생성한 청크 수
		//----------------------------------------------------------------
		static int GetCreateChunkNum()
		{
			return s_nodePool.GetAllocChunkPoolCreateNum();
		}
		static int GetInPoolChunkNum()
		{
			return s_nodePool.GetAllocChunkPoolSize();
		}
	private:
		//----------------------------------------------------------------
		// 헤드 (디큐)
		//----------------------------------------------------------------
		ALIGN64 stNode* volatile	_head;
		//----------------------------------------------------------------
		// 테일 (인큐)
		//----------------------------------------------------------------
		ALIGN64 stNode * volatile	_tail;
		//----------------------------------------------------------------
		// 전용 nullptr, 이게 달라야 큐끼리 문제가 없다.
		// 왜냐하면 기존 방식은 nullptr만 보고 인큐했기 때문
		//----------------------------------------------------------------
		ALIGN64 stNode*				_pMyNull;
		//----------------------------------------------------------------
		// 사이즈
		//----------------------------------------------------------------
		ALIGN64 volatile long 		_size;
		//----------------------------------------------------------------
		// 큐 최대 사이즈
		//----------------------------------------------------------------
		int					_maxSize;

		//----------------------------------------------------------------
		// 전용 노드 풀, 전용 카운터 (nullptr을 다르게 해야 큐끼리 문제 x)
		//----------------------------------------------------------------
		inline static unsigned long s_counter = 0;
		inline static CTlsObjectPool<stNode, LOCKFREE_QUEUE_POOL_NUM, TLS_OBJECTPOOL_USE_RAW> s_nodePool;
	};
}



#endif 