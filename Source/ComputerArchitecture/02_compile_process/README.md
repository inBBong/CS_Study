# 컴파일 과정 실습

블로그: [컴퓨터 구조_명령어](https://bb0ng2.tistory.com/31)

소스 코드가 실행 파일이 되기까지 거치는 4단계를 직접 확인하는 실습입니다. 아래 명령은 이 폴더(`Source/ComputerArchitecture/02_compile_process`)에서 실행합니다.

## MSVC (Developer PowerShell for VS 2022)

| 단계 | 명령 | 결과 파일 |
|---|---|---|
| 전처리 | `cl /P /I ..\..\common main.cpp` | `main.i` |
| 컴파일 | `cl /c /FA /utf-8 /I ..\..\common main.cpp` | `main.asm` (어셈블리어) |
| 어셈블 | `cl /c /utf-8 /I ..\..\common main.cpp calculator.cpp` | `main.obj`, `calculator.obj` |
| 링크 | `link main.obj calculator.obj /OUT:calc.exe` | `calc.exe` |

## g++ / clang++

| 단계 | 명령 | 결과 파일 |
|---|---|---|
| 전처리 | `g++ -E -I../../common main.cpp -o main.i` | `main.i` |
| 컴파일 | `g++ -S -I../../common main.cpp -o main.s` | `main.s` |
| 어셈블 | `g++ -c -I../../common main.cpp calculator.cpp` | `main.o`, `calculator.o` |
| 링크 | `g++ main.o calculator.o -o calc` | `calc` |

## 확인할 것

1. `main.i`를 열어 보세요. 주석이 사라졌고, `#include`한 헤더 내용이 붙어 있고, `SQUARE(5)`가 `((5) * (5))`로 바뀌어 있습니다.
2. `main.asm`(`main.s`)에서 `add`를 호출하는 `call` 명령어를 찾아보세요. 블로그의 연산 코드 유형 중 CALL에 해당합니다.
3. `main.obj`만으로는 실행 파일을 만들 수 없습니다. `add`의 정의가 `calculator.obj`에 있기 때문입니다.
4. 조건부 컴파일: `/DDEBUG_LOG`(g++는 `-DDEBUG_LOG`)를 붙여 빌드하면 디버그 로그 줄이 생깁니다.
5. 링크 에러: `calculator.cpp`를 `/DLINK_ERROR_DEMO`(`-DLINK_ERROR_DEMO`)로 컴파일하면 `multiply`의 정의가 빠집니다. 컴파일은 되지만 링크 단계에서 `LNK2019 확인할 수 없는 외부 기호`(g++: `undefined reference`) 에러가 납니다.
