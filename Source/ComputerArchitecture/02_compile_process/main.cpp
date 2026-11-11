// [컴퓨터 구조] 명령어 - 컴파일 과정 실습
// 블로그: https://bb0ng2.tistory.com/31
//
// 이 폴더의 README.md에 있는 명령어로 단계별 결과 파일을 직접 만들어 보자.
//   전처리(.i) -> 컴파일(.asm/.s) -> 어셈블(.obj/.o) -> 링크(.exe)
#include <cstdio>

#include "calculator.h"
#include "cs_common.h"

#define GREETING "컴파일 과정 실습"

int main() {
    cs::init_console();
    // 이 주석은 전처리 단계에서 사라진다.
    std::printf("%s\n", GREETING);

#ifdef DEBUG_LOG
    // DEBUG_LOG가 정의되었을 때만 이 코드가 포함된다. (조건부 컴파일)
    std::printf("[DEBUG] 디버그 로그가 켜져 있습니다.\n");
#endif

    std::printf("add(2, 3)      = %d\n", add(2, 3));
    std::printf("SQUARE(5)      = %d\n", SQUARE(5));  // 전처리 후: ((5) * (5))
    std::printf("multiply(4, 5) = %d\n", multiply(4, 5));
    return 0;
}
