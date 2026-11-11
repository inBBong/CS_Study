# CS 실습 예제 (C++)

블로그 [bb0ng2.tistory.com/category/CS](https://bb0ng2.tistory.com/category/CS)에 정리한 이론을 C++ 코드로 직접 돌려 보는 실습입니다. 글 하나마다 실습 파일이 하나 있습니다.

## 빌드와 실행

C++20 컴파일러(Visual Studio 2022, g++ 10 이상, clang 12 이상)와 CMake 3.20 이상이 필요합니다.

```powershell
# 저장소 루트에서
cmake -S Source -B Source/build
cmake --build Source/build --config Release

# 실행 파일은 Source/build/bin에 모입니다
Source/build/bin/CA_03_cpu_simulator.exe
```

- 성능 측정 예제(04, 05, OS_01)는 꼭 **Release**로 빌드해서 실행하세요. Debug에서는 최적화가 꺼져 결과가 다르게 나옵니다.
- Visual Studio에서는 `Source` 폴더를 열면 CMake 프로젝트로 인식되고, 실행 대상을 골라 F5로 실행할 수 있습니다.

## 컴퓨터 구조

| 파일 | 블로그 글 | 실습 내용 |
|---|---|---|
| [01_data.cpp](ComputerArchitecture/01_data.cpp) | [데이터](https://bb0ng2.tistory.com/29) | 2진수/16진수 변환, 2의 보수, 오버플로, ASCII와 패리티 비트, UTF-8/16/32 바이트 수, 한글 초성·중성·종성 분해 |
| [02_instruction.cpp](ComputerArchitecture/02_instruction.cpp) | [명령어](https://bb0ng2.tistory.com/31) | 연산 코드 + 오퍼랜드 비트 인코딩, 주소 지정 방식 8가지의 유효 주소와 메모리 접근 횟수 비교 |
| [02_compile_process/](ComputerArchitecture/02_compile_process/README.md) | [명령어](https://bb0ng2.tistory.com/31) | 전처리 → 컴파일 → 어셈블 → 링크 단계별 결과 파일 직접 만들기, 링크 에러 재현 |
| [03_cpu_simulator.cpp](ComputerArchitecture/03_cpu_simulator.cpp) | [CPU의 작동 원리](https://bb0ng2.tistory.com/33) | 8비트 CPU 시뮬레이터: PC/IR/MAR/MBR, 인출 사이클, ALU 플래그, CALL/RET, 인터럽트 마스크·백업·벡터·ISR |
| [04_cpu_performance.cpp](ComputerArchitecture/04_cpu_performance.cpp) | [CPU 성능 향상 기법](https://bb0ng2.tistory.com/34) | 파이프라인과 데이터 위험, 비순차 실행, 분기 예측 측정, 멀티스레드 속도 향상과 작업 분배 |
| [05_memory_cache.cpp](ComputerArchitecture/05_memory_cache.cpp) | [메모리와 캐시 메모리](https://bb0ng2.tistory.com/36) | MMU 베이스/한계 레지스터, L1·L2·L3·RAM 지연 시간 측정, 행 우선/열 우선 순회, 캐시 적중률 시뮬레이터 |
| [06_secondary_storage.cpp](ComputerArchitecture/06_secondary_storage.cpp) | [보조기억장치](https://bb0ng2.tistory.com/42) | HDD 접근 시간 계산, 실린더(LBA→CHS), 플래시 페이지 상태와 가비지 컬렉션, RAID 0/1/5와 XOR 패리티 복구 |
| [07_io_devices.cpp](ComputerArchitecture/07_io_devices.cpp) | [입출력장치](https://bb0ng2.tistory.com/43) | 장치 컨트롤러 레지스터, 폴링 vs 인터럽트, DMA와 사이클 스틸링, 인터럽트 우선순위와 NMI, 메모리 맵 입출력 |

[컴퓨터 구조 시작하기](https://bb0ng2.tistory.com/28)에서 다룬 CPU·메모리·버스 개념은 03 CPU 시뮬레이터에 함께 들어 있습니다.

## 운영체제

| 파일 | 블로그 글 | 실습 내용 |
|---|---|---|
| [01_os_basics.cpp](OperatingSystem/01_os_basics.cpp) | [운영체제 시작하기](https://bb0ng2.tistory.com/45) | 이중 모드와 시스템 호출 흐름, 실제 시스템 호출 비용 측정(버퍼 유무), 운영체제가 관리하는 자원 |
| [02_process_thread.cpp](OperatingSystem/02_process_thread.cpp) | [프로세스와 스레드](https://bb0ng2.tistory.com/47) | 코드/데이터/힙/스택 실제 주소, PCB와 상태 전이·문맥 교환, 스레드가 공유하는 것, fork / 자식 프로세스 |
| [03_cpu_scheduling.cpp](OperatingSystem/03_cpu_scheduling.cpp) | [CPU 스케줄링](https://bb0ng2.tistory.com/48) | FCFS, SJF, SRT, RR, 우선순위+에이징, 다단계 피드백 큐의 간트 차트와 대기/반환/응답 시간 비교 |
| [04_synchronization.cpp](OperatingSystem/04_synchronization.cpp) | [프로세스 동기화](https://bb0ng2.tistory.com/49) | 은행 계좌 경쟁 상태, 뮤텍스, 직접 만든 락과 test-and-set 스핀락, 세마포, 실행 순서 제어, 모니터(생산자-소비자) |
| [05_deadlock.cpp](OperatingSystem/05_deadlock.cpp) | [교착 상태](https://bb0ng2.tistory.com/50) | 식사하는 철학자 교착 상태 재현·검출·회복, 자원 할당 그래프 사이클, 예방 2가지, 은행원 알고리즘 |

## 공부하는 방법

1. 블로그 글을 다시 읽고, 실행하기 전에 결과를 먼저 예측해 보세요.
2. 실행해서 예측과 다른 부분을 찾고, 그 이유를 코드의 주석에서 확인하세요.
3. 각 파일 맨 아래의 `직접 해보기`를 풀어 보세요. 코드를 바꿔 보는 것이 가장 많이 남습니다.
