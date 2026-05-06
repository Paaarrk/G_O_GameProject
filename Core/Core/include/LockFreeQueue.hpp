#ifndef __LOCKFREE_QUEUE_H__
#define __LOCKFREE_QUEUE_H__

#include "TlsObjectPool_IntrusiveList.hpp"

/////////////////////////////////////////////////////////////////////
// 4가지 테스트를 통해 결정한 방법들로 만든 락프리 큐
// 1. MPMC (정석적인 방법)
// 2. MPSC (tail의 후반이동)
/////////////////////////////////////////////////////////////////////

//#define USE_ALIGN
#ifdef USE_ALIGN
#define ALIGN_64						alignas(64)
#else
#define ALIGN_64
#endif

// 카운터로 사용할 비트 수 
#define LOCKFREE_QUEUE_COUNTER_BIT	17
// 시프트 할 횟수
#define LOCKFREE_QUEUE_SHIFT_BIT	47
// 마스크
#define LOCKFREE_QUEUE_BIT_MASK		0x00007FFF'FFFFFFFF
// 고유 풀 번호
#define LOCKFREE_QUEUE_POOL_NUM		0xFE00'0000

#define LockfreeQueue_GetNextCounter(ptr)			(((uint64_t)(ptr) >> LOCKFREE_QUEUE_SHIFT_BIT) + 1)
#define LockfreeQueue_AttachCounter(ptr, counter)	((stNode*)((uint64_t)(ptr) | ((counter) << LOCKFREE_QUEUE_SHIFT_BIT)))
#define LockfreeQueue_RemoveCounter(ptr)			((stNode*)((uint64_t)(ptr) & LOCKFREE_QUEUE_BIT_MASK))
#define LockfreeQueue_IsNull(ptr)					(((uint64_t)(ptr) & (~LOCKFREE_QUEUE_BIT_MASK)) == (~LOCKFREE_QUEUE_BIT_MASK))

namespace Core
{
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
			if (_size >= _maxSize)
				return false;

			//----------------------------------------------------------------------
			// 새 노드 생성
			//----------------------------------------------------------------------
			stNode* newNode = s_nodePool.Alloc();
			newNode->next = _pMyNull;
			newNode->data = data;

			while (1)
			{
				//----------------------------------------------------------------------
				// tail 읽기 및 세팅
				//----------------------------------------------------------------------
				stNode* tail = _tail;
				uint64_t counter = LockfreeQueue_GetNextCounter(tail);
				stNode* nextTail = LockfreeQueue_AttachCounter(newNode, counter);
				stNode* next = LockfreeQueue_RemoveCounter(tail)->next;
				if (_InterlockedCompareExchangePointer((void* volatile*)&(LockfreeQueue_RemoveCounter(tail)->next), (void*)newNode, (void*)_pMyNull) == _pMyNull)
				{
					_InterlockedIncrement(&_size);
					_InterlockedCompareExchangePointer((void* volatile*)&_tail, (void*)nextTail, (void*)tail);
					break;
				}
				else
				{
					//----------------------------------------------------------------------
					// 못 붙였으면 tail 옮겨주기 시도
					//----------------------------------------------------------------------
					stNode* tail_next = LockfreeQueue_RemoveCounter(tail)->next;
					if (tail_next != _pMyNull)
					{
						nextTail = LockfreeQueue_AttachCounter(tail_next, counter);
						_InterlockedCompareExchangePointer((void* volatile*)&_tail, (void*)nextTail, (void*)tail);
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
			//----------------------------------------------------------------------
			// 새 노드 생성
			//----------------------------------------------------------------------
			stNode* newNode = s_nodePool.Alloc();
			newNode->next = _pMyNull;
			newNode->data = data;

			while (1)
			{
				//----------------------------------------------------------------------
				// tail 읽기 및 세팅
				//----------------------------------------------------------------------
				stNode* tail = _tail;
				uint64_t counter = LockfreeQueue_GetNextCounter(tail);
				stNode* nextTail = LockfreeQueue_AttachCounter(newNode, counter);
				stNode* next = LockfreeQueue_RemoveCounter(tail)->next;
				if (_InterlockedCompareExchangePointer((void* volatile*)&(LockfreeQueue_RemoveCounter(tail)->next), (void*)newNode, (void*)_pMyNull) == _pMyNull)
				{
					_InterlockedIncrement(&_size);
					_InterlockedCompareExchangePointer((void* volatile*)&_tail, (void*)nextTail, (void*)tail);
					break;
				}
				else
				{
					//----------------------------------------------------------------------
					// 못 붙였으면 tail 옮겨주기 시도
					//----------------------------------------------------------------------
					stNode* tail_next = LockfreeQueue_RemoveCounter(tail)->next;
					if (tail_next != _pMyNull)
					{
						nextTail = LockfreeQueue_AttachCounter(tail_next, counter);
						_InterlockedCompareExchangePointer((void* volatile*)&_tail, (void*)nextTail, (void*)tail);
					}
				}

			}

			return;
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
				uint64_t counter = LockfreeQueue_GetNextCounter(head);
				ret = LockfreeQueue_RemoveCounter(head);
				ret_next = ret->next;
				//----------------------------------------------------------
				// 1. 유효한 next인지를 확인
				// [1] head를 읽고 자다깻는데 재사용되서 next가 null 
				//     (어떤 큐가 재사용했는지는 중요하지 않음)
				//     => head는 무조건 바껴있으므로
				// [2] 진짜로 큐가 비어서
				// 
				// ** 왜 비교필요? ret_next에서 데이터를 꺼낼건데 null이면 
				// 뻑나니까
				//----------------------------------------------------------
				if (LockfreeQueue_IsNull(ret_next))
				{
					if (head != _head)
						continue;	// [1]번상황
					return false;	// [2]번상황
				}

				//----------------------------------------------------------
				// head가 next를 추월하는지 확인
				//----------------------------------------------------------
				stNode* tail = _tail;
				if (ret == LockfreeQueue_RemoveCounter(tail))
				{
					stNode* tail_next = LockfreeQueue_RemoveCounter(tail)->next;
					if (tail_next != _pMyNull)
					{
						uint64_t tail_counter = LockfreeQueue_GetNextCounter(tail);
						stNode* nextTail = LockfreeQueue_AttachCounter(tail_next, tail_counter);
						_InterlockedCompareExchangePointer((void* volatile*)&_tail, (void*)nextTail, (void*)tail);
					}
				}
				next = LockfreeQueue_AttachCounter(ret_next, counter);
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
		// Info: 디큐 (for MPSC), 소비자 1개일때
		// Parameter: T& data (데이터 받을 메모리)
		// Return: 성송시 true, 큐 비어있으면 false
		//----------------------------------------------------------------
		bool Dequeue_Single(T& data)
		{
			stNode* head;
			stNode* next;
			//------------------------------------------------------------
			// head 이동 및 큐검사, 데이터 빼기
			//------------------------------------------------------------
			head = _head;
			next = head->next;
			if (LockfreeQueue_IsNull(next))
			{
				return false;	// 빈 큐
			}

			data = next->data;
			_head = next;
			_InterlockedDecrement(&_size);

			//------------------------------------------------------------
			// ** 반환 직전 tail이동 **
			// . 내가 역전했고, tail이 그대로라면 CAS성공
			// . 남이 이미 밀었으면 CAS실패
			// . 최대한 늦게해서 접근 안하기?
			//------------------------------------------------------------
			stNode* tail = _tail;
			if(head == LockfreeQueue_RemoveCounter(tail))
			{
				stNode* tail_next = LockfreeQueue_RemoveCounter(tail)->next;
				if (tail_next != _pMyNull)
				{
					uint64_t tail_counter = LockfreeQueue_GetNextCounter(tail);
					stNode* nextTail = LockfreeQueue_AttachCounter(tail_next, tail_counter);
					_InterlockedCompareExchangePointer((void* volatile*)&_tail, (void*)nextTail, (void*)tail);
				}
			}

			s_nodePool.Free(head);
			return true;
		}


		//----------------------------------------------------------------
		// 사이즈 얻기, 참고용이다
		//----------------------------------------------------------------
		int GetSize() const
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
			//s_nodePool.Free(LockfreeQueue_RemoveCounter(_head));
			//_head = nullptr;
			//_tail = nullptr;
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
		// Read Write
		//----------------------------------------------------------------
		ALIGN_64 stNode* volatile	_head;
		ALIGN_64 stNode * volatile	_tail;
		ALIGN_64 volatile long 		_size;
		//----------------------------------------------------------------
		// Read Only
		//----------------------------------------------------------------
		ALIGN_64 stNode*			_pMyNull;
		int							_maxSize;

		//----------------------------------------------------------------
		// 전용 노드 풀, 전용 카운터 (nullptr을 다르게 해야 큐끼리 문제 x)
		//----------------------------------------------------------------
		inline static unsigned long s_counter = 0;
		inline static CTlsObjectPool<stNode, LOCKFREE_QUEUE_POOL_NUM, TLS_OBJECTPOOL_USE_RAW> s_nodePool;
	};
}



#endif 