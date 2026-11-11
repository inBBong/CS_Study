// 모든 실습 파일이 함께 쓰는 작은 도우미 모음
#pragma once

#include <chrono>
#include <iostream>
#include <string>

#ifdef _WIN32
// <windows.h> 전체를 포함하면 IN, OUT 같은 매크로가 딸려 오므로 필요한 함수만 선언한다.
extern "C" __declspec(dllimport) int __stdcall SetConsoleOutputCP(unsigned int codePage);
#endif

namespace cs {

// Windows 콘솔에서 한글(UTF-8)이 깨지지 않게 출력 코드 페이지를 바꾼다.
inline void init_console() {
#ifdef _WIN32
    SetConsoleOutputCP(65001);  // CP_UTF8
#endif
}

inline void title(const std::string& text) {
    std::cout << "\n" << std::string(64, '=') << "\n  " << text << "\n" << std::string(64, '=') << "\n";
}

inline void section(const std::string& text) {
    std::cout << "\n--- " << text << " ---\n";
}

// 한글은 화면에서 두 칸을 차지하므로 std::setw로는 표가 어긋난다.
// 화면 폭 기준으로 오른쪽에 공백을 채워 width 칸을 맞춘다.
inline std::string pad(const std::string& text, int width) {
    int columns = 0;
    for (size_t i = 0; i < text.size();) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        int bytes = (c < 0x80) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
        columns += (bytes >= 3) ? 2 : 1;
        i += bytes;
    }
    return text + std::string(columns < width ? width - columns : 0, ' ');
}

// 경과 시간을 재는 간단한 스톱워치
class Stopwatch {
public:
    Stopwatch() : start_(std::chrono::steady_clock::now()) {}
    void reset() { start_ = std::chrono::steady_clock::now(); }
    double ms() const {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start_).count();
    }

private:
    std::chrono::steady_clock::time_point start_;
};

}  // namespace cs
