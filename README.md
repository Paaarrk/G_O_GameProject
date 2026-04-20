# Netlibrary Refactoring And Simple MMO Online Project
1) 기존 라이브러리 리팩토링

2) 게임 프로젝트

# Refactoring

| 기간 | 내용 | 상세 링크 |
|-----|-----|----|
| 4. 14 ~ 4.19 | 락프리 큐 개선 | [[개선 사항](#lock-free-details)] |
| 4. 20 | SendPost() 개선 | [[개선 사항](#send-post-details)] |

# Game Project


# ThirdPartyLib
1) Tacopie
2) cppredis
3) MySQL Connector C

# Details

<div id="lock-free-details"></div>

### 1. 락 프리큐 개선
#### < 목표 >
- 불필요 분기 제거로 CPU 분기 예측 실패 가능성 줄이기
- 불필요 #LOCK prefix제거로 false sharing 가능성 줄이기
- 가독성 강화
<details>
<summary> 락프리 큐 가독성 강화 및 최적화  </summary>

### 1. MPMC 락프리 큐 4가지 방식의 코드 논리 비교 
#### 1. 기존 코드
- Dequeue의 CAS루프 내에서 필요시 tail 보조이동, Dequeue 종료 전 1회 더 필요 시 tail 보조이동
- tail과 size가 같이 움직임
- 수정해 나가면서 작성한 코드라 지저분함.

#### 2. New 1 (정석)
- 정석적인 락프리 큐
- Dequeue의 tail이동은 필요시 수행, 이동이 끝나면 밖으로 나옴

#### 3. New 2 (새로운 아이디어)
- CAS에서 tail, head 두개의 변수에 접근하는 것을 막는 것을 목표
- head의 tail 역전을 허용
- 단 이로 인해 MPMC에서는 노드를 꺼내고 반환하기 전 tail을 끝까지 밀어야함
- 역전 여부를 판단할 수 없어 내가 역전하지 않았어도 tail이 끝을 가리키지 않으면 끝까지 밀어줌
- -> MPSC라면 1회만 밀어줘도 괜찮으므로 (소비자가 1명이라) 좋은 방법이 될 것이라고 생각함

#### 4. New 3 (기존 코드의 개선판)
- 작동 방식은 기존과 같음
- head의 null판단을 바꿔 분기를 줄임

### 2. 측정 결과
| 테스트 환경 | 내용 |
|--|--|
| Builds | Release (O1) |
| OS | Windows |
| CPU | i7-12700H (3.5GHz / 14Core 20Thread) |

#### < 테스트 방법 >
- 4개의 생산자, 4개의 소비자
- 생성된 스레드는 while(!flag) _mm_pause()로 대기,
- 생성이 끝나면 flag를 올려주고 거의 동시에 모든 스레드가 디큐
- 4개의 생산자는 각 1억개 인큐 후 종료
- 4개의 소비자는 계속 경쟁하면서 디큐 
- 전체 수행시간을 스레드별로 측정 후 마지막에 합침
- 인큐는 전체적인 평균 시간을 타겟으로 함
- 디큐는 같이 경쟁하므로 누계평균 보다는 스레드 별 가장 빨리 끝낸 시간과 오래걸린 시간을 봄 

#### < 모든 멤버를 붙여놓고 측정 (no align) >
| 측정 항목 | 기존 코드 | New 1 | New 2 | New 3 |
|--|--|--|--|--|
| Enqueue Avg (μs) | 0.51 | 0.58 | 0.64 | 0.67 |
| Total Dequeue Time (ms) | 69885 ~ 70428 | 69547 ~ 70236 | 88543 ~ 88684 | 83709 ~ 83803 | 

#### < 모든 멤버를 띄워놓고 측정 (alignas(64)) >
| 측정 항목 | 기존 코드 | New 1 | New 2 | New 3 |
|--|--|--|--|--|
| Enqueue Avg (μs) | 0.46 | 0.31 | 0.31 | 0.35 |
| Total Dequeue Time (ms) | 60940 ~ 61555 | 41057 ~ 41995 | 34873 ~ 56402 | 52808 ~ 53426 | 

#### < 결과 해석 >
1. align이 없다면
- false sharing의 빈번함으로 예측이 어려움 (실제 테스트 결과도 실행 시 변동이 심한 편)
- 전체적으로 뭐가 좋다고 말하기 어려움

2. align의 경우
- Dequeue의 tail 보조 이동을 Dequeue의 루프내, 외부 두번하는 경우 Enqueue가 상대적으로 느림
  -> (false sharing이 추가적으로 발생하기 때문)
- Dequeue의 tail 보조 이동을 마지막에 몰아서 할 경우 Enqueue는 비슷하나 Dequeue 속도 편차가 심함
  -> Enqueue는 Dequeue의 희생으로 tail이동을 거의 안하고 인큐를 수행할 수 있지만, Dequeue는 여러 스레드가 다같이 tail이동을 하려하고 캐시라인 핑퐁은 심해짐, 노드 반환은 느려지고 그만큼 풀 사용량이 늘어남

#### < 결론 >
1. MPMC 의 방향은 New 1의 방향 (정석 방법)
2. MPSC 의 방향은 New 2의 방향 (tail의 역전 여부를 알 수 없기에 여러번 수행되는 tail보조 이동을 단 1회로 줄여볼 수 있음. head를 움직이는 주체가 1명이라서 역전도 최대 1칸이기 때문이다.)
3. Alignas의 사용은 위험하다고 생각되지만, 내 네트워크 라이브러리는 1개의 send overlapped를 사용하기 때문에 큐에서 반복적으로 꺼내서 한번에 보내기 때문에 퀀텀시간동안 큐만 건드릴 가능성이 높아 적용해 보려 한다.
4. alignas의 사용 방향은 tail, pMyNull  /  head  /  size, maxsize 로 가려고 한다!!
- pMyNull을 보고 tail뒤에 붙임 (거의 같이 읽는 변수)
- head는 단독으로 가기
- size도 단독으로, 다만 size < maxsize 비교 시 둘 다 읽으므로 붙이자
- 총 192바이트


#### < 테스트 결과 txt >

<details>
<summary>  </summary>

```text
<align>

<Base> // 더러운 코드

Per Threads Enqueue --------------------------
[Thread  0] | count:  100000000 | time:   61542.18ms | avg:       0.62us |
[Thread  1] | count:  100000000 | time:   41627.59ms | avg:       0.42us |
[Thread  2] | count:  100000000 | time:   33636.55ms | avg:       0.34us |
[Thread  3] | count:  100000000 | time:   47896.65ms | avg:       0.48us |

[    Total] | count:  400000000 | time:  184702.98ms | avg:       0.46us |


Per Threads Dequeue --------------------------
[Thread  0] | count:  290635762 | time:   61555.33ms | avg:       0.21us |
[Thread  1] | count:   54528069 | time:   61300.17ms | avg:       1.12us |
[Thread  2] | count:   27011840 | time:   61258.25ms | avg:       2.27us |
[Thread  3] | count:   27824329 | time:   60940.59ms | avg:       2.19us |

[    Total] | count:  400000000 | time:  245054.34ms | avg:       0.61us |

LeftQueueSize: 0


<New1> // 정석적 큐

Per Threads Enqueue --------------------------
[Thread  0] | count:  100000000 | time:   41202.50ms | avg:       0.41us |
[Thread  1] | count:  100000000 | time:   27995.86ms | avg:       0.28us |
[Thread  2] | count:  100000000 | time:   22350.66ms | avg:       0.22us |
[Thread  3] | count:  100000000 | time:   31108.01ms | avg:       0.31us |

[    Total] | count:  400000000 | time:  122657.03ms | avg:       0.31us |


Per Threads Dequeue --------------------------
[Thread  0] | count:  243869197 | time:   41995.05ms | avg:       0.17us |
[Thread  1] | count:   95681545 | time:   41995.07ms | avg:       0.44us |
[Thread  2] | count:   34511435 | time:   41612.85ms | avg:       1.21us |
[Thread  3] | count:   25937823 | time:   41057.79ms | avg:       1.58us |

[    Total] | count:  400000000 | time:  166660.75ms | avg:       0.42us |

LeftQueueSize: 0


<New2> // tail 마지막에 몰아서 이동

Per Threads Enqueue --------------------------
[Thread  0] | count:  100000000 | time:   13677.05ms | avg:       0.14us |
[Thread  1] | count:  100000000 | time:   34556.77ms | avg:       0.35us |
[Thread  2] | count:  100000000 | time:   39290.19ms | avg:       0.39us |
[Thread  3] | count:  100000000 | time:   38412.12ms | avg:       0.38us |

[    Total] | count:  400000000 | time:  125936.13ms | avg:       0.31us |


Per Threads Dequeue --------------------------
[Thread  0] | count:  138884385 | time:   56402.09ms | avg:       0.41us |
[Thread  1] | count:   72898869 | time:   41920.34ms | avg:       0.58us |
[Thread  2] | count:  154189235 | time:   41432.20ms | avg:       0.27us |
[Thread  3] | count:   34027511 | time:   34873.51ms | avg:       1.02us |

[    Total] | count:  400000000 | time:  174628.13ms | avg:       0.44us |

LeftQueueSize: 0


<New3> // 정석적 큐 + tail이동 후반 1회 추가

Per Threads Enqueue --------------------------
[Thread  0] | count:  100000000 | time:   47746.00ms | avg:       0.48us |
[Thread  1] | count:  100000000 | time:   31584.24ms | avg:       0.32us |
[Thread  2] | count:  100000000 | time:   25856.14ms | avg:       0.26us |
[Thread  3] | count:  100000000 | time:   36077.14ms | avg:       0.36us |

[    Total] | count:  400000000 | time:  141263.52ms | avg:       0.35us |


Per Threads Dequeue --------------------------
[Thread  0] | count:  181427690 | time:   53426.75ms | avg:       0.29us |
[Thread  1] | count:  104228869 | time:   53145.55ms | avg:       0.51us |
[Thread  2] | count:   54704704 | time:   53004.92ms | avg:       0.97us |
[Thread  3] | count:   59638737 | time:   52808.39ms | avg:       0.89us |

[    Total] | count:  400000000 | time:  212385.62ms | avg:       0.53us |

LeftQueueSize: 0




< noalign >

< Base > // 더러운 코드

Per Threads Enqueue --------------------------
[Thread  0] | count:  100000000 | time:   47285.54ms | avg:       0.47us |
[Thread  1] | count:  100000000 | time:   70409.61ms | avg:       0.70us |
[Thread  2] | count:  100000000 | time:   31586.06ms | avg:       0.32us |
[Thread  3] | count:  100000000 | time:   54296.58ms | avg:       0.54us |

[    Total] | count:  400000000 | time:  203577.79ms | avg:       0.51us |


Per Threads Dequeue --------------------------
[Thread  0] | count:   58822418 | time:   70428.79ms | avg:       1.20us |
[Thread  1] | count:   29404123 | time:   70361.04ms | avg:       2.39us |
[Thread  2] | count:   20069506 | time:   70236.09ms | avg:       3.50us |
[Thread  3] | count:  291703953 | time:   69885.71ms | avg:       0.24us |

[    Total] | count:  400000000 | time:  280911.63ms | avg:       0.70us |

LeftQueueSize: 0



< New 1 > // 정석 코드

Per Threads Enqueue --------------------------
[Thread  0] | count:  100000000 | time:   53629.36ms | avg:       0.54us |
[Thread  1] | count:  100000000 | time:   69204.99ms | avg:       0.69us |
[Thread  2] | count:  100000000 | time:   53544.17ms | avg:       0.54us |
[Thread  3] | count:  100000000 | time:   57040.15ms | avg:       0.57us |

[    Total] | count:  400000000 | time:  233418.67ms | avg:       0.58us |


Per Threads Dequeue --------------------------
[Thread  0] | count:   35732299 | time:   70236.35ms | avg:       1.97us |
[Thread  1] | count:  226226960 | time:   70142.62ms | avg:       0.31us |
[Thread  2] | count:   63897523 | time:   70046.09ms | avg:       1.10us |
[Thread  3] | count:   74143218 | time:   69547.63ms | avg:       0.94us |

[    Total] | count:  400000000 | time:  279972.70ms | avg:       0.70us |

LeftQueueSize: 0


< New 2 > // tail맨 마지막에 보조이동 한번에, head역전 허용

Per Threads Enqueue --------------------------
[Thread  0] | count:  100000000 | time:   39693.58ms | avg:       0.40us |
[Thread  1] | count:  100000000 | time:   59979.79ms | avg:       0.60us |
[Thread  2] | count:  100000000 | time:   68372.69ms | avg:       0.68us |
[Thread  3] | count:  100000000 | time:   87823.43ms | avg:       0.88us |

[    Total] | count:  400000000 | time:  255869.48ms | avg:       0.64us |


Per Threads Dequeue --------------------------
[Thread  0] | count:   51796290 | time:   88684.26ms | avg:       1.71us |
[Thread  1] | count:   75584202 | time:   88684.25ms | avg:       1.17us |
[Thread  2] | count:  226137389 | time:   88637.52ms | avg:       0.39us |
[Thread  3] | count:   46482119 | time:   88543.60ms | avg:       1.90us |

[    Total] | count:  400000000 | time:  354549.63ms | avg:       0.89us |

LeftQueueSize: 0


< New 3 > // 더러운 코드의 개선판 (구조는 같음, 정석에서 마지막에 tail보조이동 1회 추가)

Per Threads Enqueue --------------------------
[Thread  0] | count:  100000000 | time:   59352.38ms | avg:       0.59us |
[Thread  1] | count:  100000000 | time:   56787.43ms | avg:       0.57us |
[Thread  2] | count:  100000000 | time:   67061.52ms | avg:       0.67us |
[Thread  3] | count:  100000000 | time:   83560.21ms | avg:       0.84us |

[    Total] | count:  400000000 | time:  266761.55ms | avg:       0.67us |


Per Threads Dequeue --------------------------
[Thread  0] | count:   34820487 | time:   83803.31ms | avg:       2.41us |
[Thread  1] | count:   31632632 | time:   83803.20ms | avg:       2.65us |
[Thread  2] | count:   44558174 | time:   83756.48ms | avg:       1.88us |
[Thread  3] | count:  288988707 | time:   83709.00ms | avg:       0.29us |

[    Total] | count:  400000000 | time:  335071.99ms | avg:       0.84us |

LeftQueueSize: 0
```

</details>



</details>
<div id="lock-free-details"></div><details>
<summary> 교체 전 후 테스트 비교  </summary>

### 1. 테스트 환경
| 테스트 환경 | 내용 |
|--|--|
| Builds | Release (O1) |
| OS | Windows |
| CPU | i7-12700H (3.5GHz / 14Core 20Thread) |

### 2. 테스트 (상황 테스트, MPMC) 
#### 1) 방법
1. Enqueue 4개 스레드 / Dequeue 4개 스레드
2. 거의 동시에 시작 (flag사용, 생성된 스레드는 mm_pause로 대기)
3. 인큐는 4억회 인큐가 끝나면 release
4. 디큐는 진행 하던 중 인큐 스레드 4개가 완료됨을 인지하면 루프를 나와 큐가 빌 때 까지 디큐함.
5. 둘 다 head, tail, size, (pMyNull, maxsize) 에 align적용


#### 2) 결과
| 항목 | New | Old |
|--|--|--|
| Enqueue Avg (μs) | 0.29 (30% ▼) | 0.41 |
| Dequeue Avg (μs) | 0.45 (15% ▼) | 0.53 |
| Total Dequeue Time Distribution (ms) | 44400 ~ 44615 | 52833 ~ 53379 |

#### 3) 결과 해석 및 차이 
1. Dequeue로직의 tail이동 횟수 감소로 enqueue 경합 감소 및 속도 개선
</br> - 아랫쪽에서 한번 더 수행하던 로직을 없애 Dequeue도 빠르게, Enqueue도 빠르게
</br> - 기존에는 좀 더 도와주면 Enqueue가 빠르지 않을까 해서 넣었던 로직이였음

2. Dequeue로직의 Null 판단 최적화로 분기 1회 줄임
</br> - 기존은 _pMyNull 확인 이후 한번 더 전체적인 Null인지 확인 (상위 비트 FFFF8인지)
</br> - 현재는 초기 1회 상위 비트 FFFF8확인으로 끝냄

3. Dequeue로직의 더미노드만 있는지 확인하는 방법을 head가 tail을 추월하는지 확인하는 로직으로 변경 (분기 1회 감소)
4. 가독성 향상을 위해 비트 연산부를 매크로 함수로 치환
#### 4) 결론
1. 분기 최적화 를 통해 분기예측 실패가능성을 줄임
2. 불필요한 Interlocked 연산을 줄여 falsesharing 가능성을 줄임

### 3. 서버에 적용 테스트

1. 유의미한 차이를 찾지 못했다. 
- 세션 자체가 15000개, 유저 1만명에서 크게 경합이 생길 수 있는 상황은 아니였기 때문에 기존 코드와 비교해서 차이가 없었다.
- align을 안하는 것이 좀 더 처리가 수월했다. 경합이 잘 생기지 않아 전체적으로 캐시 라인을 띄엄띄엄 읽는것 보다 한번에 읽어오는게 더 좋은 것은 것으로 보여진다.


#### < 테스트 결과 txt >

<details>
<summary>  </summary>

```text
< 두개 모두 alignas(64) 적용 된 상태 >

< 새로 만든 거 >
Per Threads Enqueue --------------------------
[Thread  0] | count:  100000000 | time:   30810.91ms | avg:       0.31us |
[Thread  1] | count:  100000000 | time:   13040.15ms | avg:       0.13us |
[Thread  2] | count:  100000000 | time:   28260.94ms | avg:       0.28us |
[Thread  3] | count:  100000000 | time:   43249.65ms | avg:       0.43us |

[    Total] | count:  400000000 | time:  115361.64ms | avg:       0.29us |


Per Threads Dequeue --------------------------
[Thread  0] | count:  198593433 | time:   44615.30ms | avg:       0.22us |
[Thread  1] | count:  173097643 | time:   44615.16ms | avg:       0.26us |
[Thread  2] | count:   14560489 | time:   44458.91ms | avg:       3.05us |
[Thread  3] | count:   13748435 | time:   44400.64ms | avg:       3.23us |

[    Total] | count:  400000000 | time:  178090.01ms | avg:       0.45us |

LeftQueueSize: 0


< 기존 >
Per Threads Enqueue --------------------------
[Thread  0] | count:  100000000 | time:   52798.47ms | avg:       0.53us |
[Thread  1] | count:  100000000 | time:   26230.77ms | avg:       0.26us |
[Thread  2] | count:  100000000 | time:   39343.09ms | avg:       0.39us |
[Thread  3] | count:  100000000 | time:   43818.69ms | avg:       0.44us |

[    Total] | count:  400000000 | time:  162191.02ms | avg:       0.41us |


Per Threads Dequeue --------------------------
[Thread  0] | count:   43581571 | time:   53379.82ms | avg:       1.22us |
[Thread  1] | count:   28326070 | time:   53265.12ms | avg:       1.88us |
[Thread  2] | count:  277376541 | time:   53248.80ms | avg:       0.19us |
[Thread  3] | count:   50715818 | time:   52833.04ms | avg:       1.04us |

[    Total] | count:  400000000 | time:  212726.79ms | avg:       0.53us |

LeftQueueSize: 0
```

</details>
</details>


</br>

<div id="send-post-details"></div>
<details>
<summary> 네트워크 코드의 Send루프 가독성 강화  </summary>

### 1. 기존 코드의 문제

1. 기존의 SendPost는 읽기 힘든 루프 구조 (goto도 사용)
- do while로 깔끔하게 묶는 방향으로 개선
2. 기존의 Send 완료 통지 이후 플래그를 포기하고 재획득 하는 불필요 로직 존재
- 게임 로직에서 SendPacket 이후 PQCS SendPost를 덜 호출하는 방향으로 유도하기 위해 Send 완료 통지에서 플래그를 포기하지 않고 SendPost를 바로 진행 (큐에 한개라도 넣었다면 완료통지에서 바로 재진행 하도록)

### 2. 개선 결과
1. 가독성 향상, 코드 길이 감소 (WSABUF에 디큐해서 메시지를 넣는 로직)
- 300만회의 샘플을 통해 개선 부분의 루프 측정 결과 소폭 감소
- 5.6483㎲ ->  4.7559㎲

2. 실제로 게임 로직이 덜 SendPacket내부에서 PQCS SendPost 호출하는지는 (보낼 것을 큐에만 넣고 나가는지는) 게임을 만들어 Broadcast로 한 게임 루프 틱에서 여러번 같은 세션에 대해 SendPacket을 호출 할 때 측정이 가능할 것으로 보임.
- 에코 서버 기준으로는 1천만회 Send호출 중 기존 코드는 1.0054%, 개선 코드는 1.0031%로 거의 비슷한 수치를 보임.

</details>

