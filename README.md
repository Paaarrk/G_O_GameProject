# Netlibrary Refactoring And Goddamburg Online Project
1) 기존 라이브러리 리팩토링

2) 게임 프로젝트

# Refactoring

| 기간 | 내용 |
|-----|-----|
| 4. 14 ~  | 락프리 큐 가독성 강화 및 최적화 [[상세 내용 확인](#lock-free-details)] |

# Game Project


# Details

<div id="lock-free-details"></div><details>
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
| CPU | i7-12700H (3.0GHz / 14Core 20Thread) |

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

</br>

<div id="lock-free-details"></div><details>
<summary> 네트워크 코드의 Send루프 가독성 강화  </summary>
