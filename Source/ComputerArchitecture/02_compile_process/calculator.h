// 헤더 파일: 함수의 '선언'만 있다. 전처리 단계에서 #include한 파일에 그대로 복사된다.
#pragma once

// 매크로: 전처리 단계에서 글자 그대로 치환된다. (main.i 파일에서 확인해 보기)
#define SQUARE(x) ((x) * (x))

int add(int a, int b);
int multiply(int a, int b);
