// 함수의 '정의'가 있는 파일. 따로 컴파일되어 calculator.obj(.o)라는 목적 파일이 된다.
#include "calculator.h"

int add(int a, int b) {
    return a + b;
}

// LINK_ERROR_DEMO를 정의하고 빌드하면 multiply의 정의가 사라진다.
// 컴파일은 성공하지만, 링크 단계에서 "확인할 수 없는 외부 기호"(undefined reference) 에러가 난다.
#ifndef LINK_ERROR_DEMO
int multiply(int a, int b) {
    return a * b;
}
#endif
