// [컴퓨터 구조] 데이터
// 블로그: https://bb0ng2.tistory.com/29
//
// 실습 내용
//   1. 정보 단위: 비트, 바이트, 워드
//   2. 2진수와 16진수 (16진수 한 자리 = 4비트)
//   3. 2의 보수로 음수 표현하기, 오버플로
//   4. ASCII 코드 포인트와 패리티 비트
//   5. 유니코드: 같은 문자가 UTF-8 / UTF-16 / UTF-32에서 몇 바이트인지
//   6. 한글 완성형 코드 포인트를 초성·중성·종성으로 분해하고 조합하기
#include "cs_common.h"

#include <bit>
#include <bitset>
#include <climits>
#include <cstdint>
#include <iomanip>
#include <string>
#include <vector>

using std::cout;

// 0 이상의 정수를 원하는 진법의 문자열로 바꾼다. (나머지를 거꾸로 이어 붙이는 방법)
std::string to_base(uint64_t value, int base) {
    if (value == 0) return "0";
    const char* digits = "0123456789ABCDEF";
    std::string result;
    while (value > 0) {
        result.insert(result.begin(), digits[value % base]);
        value /= base;
    }
    return result;
}

// 진법 문자열을 정수로 바꾼다. (자릿값을 곱하며 누적)
uint64_t from_base(const std::string& text, int base) {
    uint64_t value = 0;
    for (char c : text) {
        int digit = (c >= '0' && c <= '9') ? c - '0' : (c & ~0x20) - 'A' + 10;  // & ~0x20: 소문자를 대문자로
        value = value * base + digit;
    }
    return value;
}

void information_units() {
    cs::section("1. 정보 단위");
    cout << "1 바이트 = " << CHAR_BIT << " 비트\n\n";
    cout << cs::pad("자료형", 12) << cs::pad("바이트", 8) << "비트\n";
    auto row = [](const char* name, size_t bytes) {
        cout << std::left << std::setw(12) << name << std::setw(8) << bytes << bytes * CHAR_BIT << "\n";
    };
    row("char", sizeof(char));
    row("short", sizeof(short));
    row("int", sizeof(int));
    row("long long", sizeof(long long));
    row("void*", sizeof(void*));
    cout << "\n포인터 크기가 " << sizeof(void*) * 8
         << "비트이므로 이 프로그램은 " << sizeof(void*) * 8
         << "비트 환경으로 빌드되었다. 워드 크기를 짐작하는 힌트가 된다.\n";
}

void number_systems() {
    cs::section("2. 2진수와 16진수");
    for (uint64_t n : {1, 2, 3, 4, 10, 26, 255}) {
        cout << std::right << std::setw(4) << n << "(10) = " << std::setw(9) << to_base(n, 2)
             << "(2) = " << to_base(n, 16) << "(16)\n";
    }
    cout << "\n문자열 -> 숫자: \"1010\"(2) = " << from_base("1010", 2) << ", \"1A\"(16) = " << from_base("1A", 16)
         << "\n";
    cout << "코드 표기법:  0b1010 = " << 0b1010 << ",  0x1A = " << 0x1A << "\n";

    // 16진수 한 자리는 정확히 4비트(니블)에 대응한다.
    uint32_t value = 0xCAFE;
    cout << "\n0xCAFE를 4비트씩 쪼개 보기:\n";
    for (int i = 3; i >= 0; --i) {
        uint32_t nibble = (value >> (i * 4)) & 0xF;
        cout << "  " << to_base(nibble, 16) << " -> " << std::bitset<4>(nibble) << "\n";
    }
    cout << "  합치면 " << std::bitset<16>(value) << "\n";
}

void twos_complement() {
    cs::section("3. 2의 보수");

    // 블로그 예시: 0110(6)의 2의 보수는 1010
    uint8_t six = 0b0110;
    uint8_t flipped = static_cast<uint8_t>(~six) & 0xF;  // 4비트만 사용
    uint8_t minus_six = (flipped + 1) & 0xF;
    cout << "0110(6) -> 비트 반전 " << std::bitset<4>(flipped) << " -> +1 -> " << std::bitset<4>(minus_six) << "\n";
    uint8_t back = ((static_cast<uint8_t>(~minus_six) & 0xF) + 1) & 0xF;
    cout << "1010에 한 번 더 2의 보수 -> " << std::bitset<4>(back) << " (원래 수로 돌아온다: -(-6) = 6)\n";

    // 같은 비트 1010도 해석에 따라 10 또는 -6이다. CPU는 부호 플래그로 이를 구분한다.
    int as_unsigned = minus_six;
    int as_signed = (minus_six & 0x8) ? minus_six - 16 : minus_six;  // 최상위 비트가 1이면 음수
    cout << "비트 1010의 해석: 부호 없는 수 " << as_unsigned << " / 2의 보수 " << as_signed << "\n\n";

    cout << "8비트(int8_t)에서의 표현:\n";
    for (int v : {5, -5, 1, -1, 127, -128}) {
        cout << "  " << std::setw(5) << v << " = " << std::bitset<8>(static_cast<uint8_t>(v)) << "\n";
    }

    // 오버플로: 표현 범위를 넘어가면 값이 한 바퀴 돌아간다.
    int8_t max_signed = 127;
    int8_t wrapped = static_cast<int8_t>(max_signed + 1);  // C++20부터 모듈러 변환이 표준으로 정해짐
    uint8_t max_unsigned = 255;
    uint8_t wrapped_u = static_cast<uint8_t>(max_unsigned + 1);
    cout << "\n오버플로: int8_t 127 + 1 = " << int(wrapped) << ",  uint8_t 255 + 1 = " << int(wrapped_u) << "\n";
}

void ascii_and_parity() {
    cs::section("4. ASCII와 패리티 비트");
    for (char c : std::string("Aa0~")) {
        cout << "  '" << c << "' 코드 포인트 = " << std::setw(3) << int(c) << " = " << std::bitset<7>(c) << "\n";
    }
    cout << "  'a' - 'A' = " << ('a' - 'A') << " -> 대소문자는 0x20 비트 하나 차이: 'g' ^ 0x20 = '"
         << static_cast<char>('g' ^ 0x20) << "'\n";

    // 짝수 패리티: 1의 개수가 짝수가 되도록 8번째 비트를 정한다.
    cout << "\n짝수 패리티 비트 붙이기:\n";
    for (char c : std::string("AC")) {
        int ones = std::popcount(static_cast<unsigned>(c));
        int parity = ones % 2;
        uint8_t with_parity = static_cast<uint8_t>((parity << 7) | c);
        cout << "  '" << c << "' " << std::bitset<7>(c) << " (1이 " << ones << "개) -> 패리티 " << parity << " -> "
             << std::bitset<8>(with_parity) << "\n";
    }
    // 전송 중 한 비트가 뒤집히면 1의 개수가 홀수가 되어 오류를 검출할 수 있다.
    uint8_t received = static_cast<uint8_t>('A' ^ 0b0000100);
    cout << "  전송 중 1비트 오류 발생: " << std::bitset<8>(received) << " -> 1의 개수 "
         << std::popcount(static_cast<unsigned>(received)) << "개(홀수) -> 오류 검출!\n";
}

// UTF-8 바이트열을 코드 포인트와 각 문자의 바이트 길이로 디코딩한다.
struct DecodedChar {
    char32_t code_point;
    std::string bytes;
};

std::vector<DecodedChar> decode_utf8(const std::string& text) {
    std::vector<DecodedChar> result;
    for (size_t i = 0; i < text.size();) {
        unsigned char lead = static_cast<unsigned char>(text[i]);
        int length = (lead < 0x80) ? 1 : (lead >> 5) == 0x6 ? 2 : (lead >> 4) == 0xE ? 3 : 4;
        char32_t cp = (length == 1) ? lead : (length == 2) ? (lead & 0x1F) : (length == 3) ? (lead & 0x0F) : (lead & 0x07);
        for (int k = 1; k < length; ++k) {
            cp = (cp << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3F);  // 이어지는 바이트는 하위 6비트만 데이터
        }
        result.push_back({cp, text.substr(i, length)});
        i += length;
    }
    return result;
}

std::string encode_utf8(char32_t cp) {
    std::string out;
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    return out;
}

void unicode() {
    cs::section("5. 유니코드와 UTF-8 / UTF-16 / UTF-32");
    std::string text = "A한😀";  // 소스 파일과 실행 문자 집합이 UTF-8 (/utf-8 옵션)
    cout << "문자  코드포인트  UTF-8 바이트          UTF-8  UTF-16  UTF-32\n";
    for (const auto& ch : decode_utf8(text)) {
        std::string hex_bytes;
        for (unsigned char b : ch.bytes) hex_bytes += to_base(b, 16) + " ";
        int utf16_bytes = (ch.code_point >= 0x10000) ? 4 : 2;  // BMP 밖은 서로게이트 쌍(2단위)
        cout << "  " << ch.bytes << "\tU+" << std::setw(6) << std::left << to_base(ch.code_point, 16)
             << std::setw(22) << hex_bytes << ch.bytes.size() << "B     " << utf16_bytes << "B      4B\n";
    }
    cout << std::right;
    cout << "\n참고: EUC-KR에서 '한'은 C7 D1 (2바이트, 완성형 2,350자 안에서만 표현 가능)\n";
    cout << "      std::u16string(u\"한\").size() = " << std::u16string(u"한").size()
         << ", std::u32string(U\"😀\").size() = " << std::u32string(U"😀").size() << "\n";
}

// 유니코드 한글 음절 = 0xAC00 + (초성 * 21 + 중성) * 28 + 종성
// 완성형 코드 안에 조합형의 원리가 수식으로 녹아 있다.
const char* kChoseong[] = {"ㄱ", "ㄲ", "ㄴ", "ㄷ", "ㄸ", "ㄹ", "ㅁ", "ㅂ", "ㅃ", "ㅅ",
                           "ㅆ", "ㅇ", "ㅈ", "ㅉ", "ㅊ", "ㅋ", "ㅌ", "ㅍ", "ㅎ"};
const char* kJungseong[] = {"ㅏ", "ㅐ", "ㅑ", "ㅒ", "ㅓ", "ㅔ", "ㅕ", "ㅖ", "ㅗ", "ㅘ", "ㅙ",
                            "ㅚ", "ㅛ", "ㅜ", "ㅝ", "ㅞ", "ㅟ", "ㅠ", "ㅡ", "ㅢ", "ㅣ"};
const char* kJongseong[] = {"(없음)", "ㄱ", "ㄲ", "ㄳ", "ㄴ", "ㄵ", "ㄶ", "ㄷ", "ㄹ", "ㄺ",
                            "ㄻ",     "ㄼ", "ㄽ", "ㄾ", "ㄿ", "ㅀ", "ㅁ", "ㅂ", "ㅄ", "ㅅ",
                            "ㅆ",     "ㅇ", "ㅈ", "ㅊ", "ㅋ", "ㅌ", "ㅍ", "ㅎ"};

void hangul() {
    cs::section("6. 한글 음절 분해와 조합");
    for (const auto& ch : decode_utf8("컴퓨터")) {
        char32_t index = ch.code_point - 0xAC00;
        int cho = static_cast<int>(index / (21 * 28));
        int jung = static_cast<int>((index % (21 * 28)) / 28);
        int jong = static_cast<int>(index % 28);
        cout << "  " << ch.bytes << " (U+" << to_base(ch.code_point, 16) << ") = 초성 " << kChoseong[cho] << " + 중성 "
             << kJungseong[jung] << " + 종성 " << kJongseong[jong] << "\n";
    }
    // ㅎ(18) + ㅏ(0) + ㄴ(4) -> '한'
    char32_t composed = 0xAC00 + (18 * 21 + 0) * 28 + 4;
    cout << "  조합: ㅎ(18) + ㅏ(0) + ㄴ(4) -> U+" << to_base(composed, 16) << " '" << encode_utf8(composed) << "'\n";
}

int main() {
    cs::init_console();
    cs::title("컴퓨터 구조 - 데이터");
    information_units();
    number_systems();
    twos_complement();
    ascii_and_parity();
    unicode();
    hangul();
    return 0;
}

// 직접 해보기
//  - to_base()로 8진수도 출력해 보고, 3비트 단위로 끊으면 2진수와 대응되는지 확인하기
//  - int16_t에서 32767 + 1, -32768 - 1의 결과와 비트 패턴 출력하기
//  - 여러분 이름을 decode_utf8()에 넣어 초성/중성/종성으로 분해하기
