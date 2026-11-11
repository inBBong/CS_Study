# CS_Study 진행 방식

블로그에 하루 한 챕터씩 CS 이론을 정리하고, 챕터마다 C++ 실습 예제를 하나씩 만드는 저장소입니다.

- 블로그: https://bb0ng2.tistory.com/category/CS
- 목표 직무: 안랩 OS기반 SW개발. 면접에서 코드 한 줄 한 줄을 설명할 수 있어야 합니다.

## 하루 진행 순서

1. 블로그에 새 챕터를 올린다.
2. Claude에게 글 주소(URL)를 보낸다.
3. Claude가 글을 읽고 실습 예제를 만든다. 빌드하고 실행해서 출력까지 확인한다.
4. 받은 코드를 직접 실행해 보고, 파일 맨 아래 `직접 해보기` 중 하나 이상을 직접 푼다.
5. 챕터 하나를 커밋 하나로 남기고 push한다.

## 예제 작성 규칙

### 파일 위치와 이름

| 분야 | 위치 | 실행 파일 이름 |
|---|---|---|
| 컴퓨터 구조 | `Source/ComputerArchitecture/NN_주제.cpp` | `CA_NN_주제.exe` |
| 운영체제 | `Source/OperatingSystem/NN_주제.cpp` | `OS_NN_주제.exe` |

- `NN`은 분야별로 이어지는 번호다. 블로그에 글을 올린 순서를 따른다.
- 새 분야(예: 네트워크, 자료구조)가 생기면 `Source/분야이름/` 폴더를 만들고 `Source/CMakeLists.txt`에 glob을 추가한다.
- `.cpp` 파일은 CMake가 자동으로 찾는다. 여러 파일로 된 실습만 `CMakeLists.txt`에 직접 등록한다(예: `02_compile_process`).

### 파일 구성

```cpp
// [운영체제] 챕터 제목
// 블로그: https://bb0ng2.tistory.com/NN
//
// 실습 내용
//   1. ...
//   2. ...
#include "cs_common.h"

int main() {
    cs::init_console();
    cs::title("운영체제 - 챕터 제목");
    ...
}

// 직접 해보기
//  - 코드를 바꿔 보는 과제 2~3개
```

### 내용

- 글에서 실제로 다룬 개념을 실습한다. 글의 예시와 숫자를 그대로 쓸 수 있으면 그대로 쓴다.
- 주제가 허락하면 시뮬레이션보다 실제 OS API를 쓴다(Win32, POSIX). 직무와 직접 이어지기 때문이다.
- 실행 결과만 봐도 개념이 보이게 출력한다. 측정값, 비교표, 상태 변화 로그 같은 것이다.
- 주석은 한글로 쓰고, 왜 그렇게 되는지를 설명한다.

### 코드 규칙

- C++20, 표준 라이브러리만 사용한다. Unix 전용 코드(`fork` 등)는 `#ifdef`로 감싸고 Windows 대체 동작을 둔다.
- 공용 도우미는 `Source/common/cs_common.h`에 있다.
  - `cs::init_console()`: Windows 콘솔에서 한글이 깨지지 않게 한다. `main` 첫 줄에서 호출한다.
  - `cs::title()`, `cs::section()`: 제목과 소제목을 출력한다.
  - `cs::pad(text, width)`: 한글이 섞인 표를 정렬한다. 한글이 들어간 칸에는 `std::setw` 대신 이것을 쓴다.
  - `cs::Stopwatch`: 시간을 잰다.
- 여러 스레드가 출력할 때는 mutex로 줄이 섞이지 않게 한다.
- 경고 없이 빌드되어야 한다(MSVC `/W4`).

## 빌드와 실행

```powershell
cmake -S Source -B Source/build
cmake --build Source/build --config Release
Source/build/bin/OS_05_deadlock.exe
```

성능을 재는 예제는 Release로 실행한다.

## 마무리 체크리스트

- [ ] 빌드 경고 0개
- [ ] 실행해서 출력이 글의 내용과 맞는지 확인
- [ ] `Source/README.md` 표에 한 줄 추가
- [ ] 커밋 메시지: `[분야] 챕터 제목 실습 추가` (예: `[운영체제] 교착 상태 실습 추가`)
