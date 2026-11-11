// [컴퓨터 구조] 명령어
// 블로그: https://bb0ng2.tistory.com/31
//
// 실습 내용
//   1. 명령어 = 연산 코드 + 오퍼랜드. 16비트 명령어를 비트 연산으로 인코딩/디코딩하기
//   2. 오퍼랜드 필드 크기에 따라 표현할 수 있는 값의 범위가 줄어드는 것 확인하기
//   3. 주소 지정 방식: 즉시 / 직접 / 간접 / 레지스터 / 레지스터 간접 / 스택 / 상대 / 베이스 레지스터
//      모든 방식이 같은 값(42)을 서로 다른 경로로 찾아가게 만들어 유효 주소와 메모리 접근 횟수를 비교한다.
//
// 컴파일 과정(전처리 -> 컴파일 -> 어셈블 -> 링크) 실습은 02_compile_process 폴더에 있다.
#include "cs_common.h"

#include <array>
#include <bitset>
#include <cstdint>
#include <iomanip>
#include <string>

using std::cout;

// ---------------------------------------------------------------
// 1. 명령어 인코딩
// ---------------------------------------------------------------
enum Opcode : uint8_t { OP_LOAD = 0x1, OP_STORE = 0x2, OP_ADD = 0x3, OP_JUMP = 0x4, OP_HALT = 0xF };

const char* opcode_name(uint8_t op) {
    switch (op) {
        case OP_LOAD: return "LOAD";
        case OP_STORE: return "STORE";
        case OP_ADD: return "ADD";
        case OP_JUMP: return "JUMP";
        case OP_HALT: return "HALT";
        default: return "???";
    }
}

// 1-주소 명령어: [연산 코드 4비트][오퍼랜드 12비트]
uint16_t encode_1addr(uint8_t op, uint16_t operand) {
    return static_cast<uint16_t>(((op & 0xF) << 12) | (operand & 0xFFF));
}

// 2-주소 명령어: [연산 코드 4비트][오퍼랜드1 6비트][오퍼랜드2 6비트]
uint16_t encode_2addr(uint8_t op, uint16_t a, uint16_t b) {
    return static_cast<uint16_t>(((op & 0xF) << 12) | ((a & 0x3F) << 6) | (b & 0x3F));
}

void instruction_encoding() {
    cs::section("1. 명령어 인코딩 (16비트)");
    uint16_t inst = encode_1addr(OP_LOAD, 1000);
    cout << "LOAD 1000 -> " << std::bitset<16>(inst) << " (0x" << std::hex << inst << std::dec << ")\n";
    cout << "  디코딩: 연산 코드 = " << opcode_name(inst >> 12) << ", 오퍼랜드 = " << (inst & 0xFFF) << "\n";
    cout << "  오퍼랜드 12비트로 표현 가능한 값: 2^12 = " << (1 << 12) << "가지\n\n";

    uint16_t inst2 = encode_2addr(OP_ADD, 10, 20);
    cout << "ADD 10, 20 -> " << std::bitset<16>(inst2) << "\n";
    cout << "  디코딩: " << opcode_name(inst2 >> 12) << " " << ((inst2 >> 6) & 0x3F) << ", " << (inst2 & 0x3F) << "\n";
    cout << "  오퍼랜드가 6비트씩이면 표현 가능한 값: 2^6 = " << (1 << 6) << "가지\n";

    // 범위를 넘는 값을 넣으면 잘려 나간다. 그래서 오퍼랜드에는 데이터 대신 '주소'를 담는다.
    uint16_t truncated = encode_2addr(OP_ADD, 100, 1);
    cout << "  ADD 100, 1을 넣으면? -> 디코딩 결과 " << ((truncated >> 6) & 0x3F) << ", " << (truncated & 0x3F)
         << " (100이 6비트에 다 들어가지 않아 잘림)\n";
}

// ---------------------------------------------------------------
// 2. 주소 지정 방식
// ---------------------------------------------------------------
struct Machine {
    std::array<int, 64> memory{};
    std::array<int, 4> reg{};  // R0 ~ R3
    int pc = 0;                // 프로그램 카운터 (다음 명령어 주소)
    int sp = 0;                // 스택 포인터
    int base = 0;              // 베이스 레지스터
    int memory_accesses = 0;

    int read_memory(int address) {
        ++memory_accesses;
        return memory.at(address);
    }
};

enum class Mode { Immediate, Direct, Indirect, Register, RegisterIndirect, Stack, Relative, BaseRegister };

struct OperandResult {
    std::string mode_name;
    std::string operand_text;
    std::string effective_address;  // 유효 주소 (없으면 "-")
    int value;
    int memory_accesses;
};

OperandResult fetch_operand(Machine m, Mode mode, int operand) {  // 복사본으로 받아 접근 횟수를 따로 센다
    m.memory_accesses = 0;
    switch (mode) {
        case Mode::Immediate:
            return {"즉시", "#" + std::to_string(operand), "-", operand, m.memory_accesses};
        case Mode::Direct: {
            int value = m.read_memory(operand);
            return {"직접", std::to_string(operand), std::to_string(operand), value, m.memory_accesses};
        }
        case Mode::Indirect: {
            int ea = m.read_memory(operand);  // 1번째 접근: 유효 주소를 읽는다
            int value = m.read_memory(ea);    // 2번째 접근: 데이터를 읽는다
            return {"간접", "@" + std::to_string(operand), std::to_string(ea), value, m.memory_accesses};
        }
        case Mode::Register:
            return {"레지스터", "R" + std::to_string(operand), "-", m.reg[operand], m.memory_accesses};
        case Mode::RegisterIndirect: {
            int ea = m.reg[operand];
            int value = m.read_memory(ea);
            return {"레지스터 간접", "(R" + std::to_string(operand) + ")", std::to_string(ea), value, m.memory_accesses};
        }
        case Mode::Stack: {
            int value = m.read_memory(m.sp);
            return {"스택", "(SP)", std::to_string(m.sp), value, m.memory_accesses};
        }
        case Mode::Relative: {
            int ea = m.pc + operand;
            int value = m.read_memory(ea);
            return {"상대(PC+변위)", std::to_string(operand), std::to_string(m.pc) + "+" + std::to_string(operand),
                    value, m.memory_accesses};
        }
        case Mode::BaseRegister: {
            int ea = m.base + operand;
            int value = m.read_memory(ea);
            return {"베이스 레지스터", std::to_string(operand), std::to_string(m.base) + "+" + std::to_string(operand),
                    value, m.memory_accesses};
        }
    }
    return {};
}

void addressing_modes() {
    cs::section("2. 주소 지정 방식");
    Machine m;
    m.memory[5] = 42;  // 우리가 찾고 싶은 데이터는 메모리 5번지의 42
    m.memory[20] = 5;  // 간접 주소 지정용: 20번지에는 '데이터의 주소'가 들어 있다
    m.reg[1] = 42;     // 레지스터 주소 지정용: 데이터가 이미 레지스터에 있다
    m.reg[2] = 5;      // 레지스터 간접용: 레지스터에 '데이터의 주소'가 있다
    m.sp = 5;          // 스택 꼭대기가 5번지
    m.pc = 3;          // 다음 명령어 주소
    m.base = 2;        // 기준 주소

    cout << "메모리 5번지 = 42, 20번지 = 5, R1 = 42, R2 = 5, SP = 5, PC = 3, BASE = 2\n\n";
    cout << cs::pad("방식", 18) << cs::pad("오퍼랜드", 10) << cs::pad("유효주소", 12) << cs::pad("값", 6)
         << "메모리 접근\n";

    const std::pair<Mode, int> cases[] = {
        {Mode::Immediate, 42}, {Mode::Direct, 5},   {Mode::Indirect, 20}, {Mode::Register, 1},
        {Mode::RegisterIndirect, 2}, {Mode::Stack, 0}, {Mode::Relative, 2}, {Mode::BaseRegister, 3},
    };
    for (auto [mode, operand] : cases) {
        OperandResult r = fetch_operand(m, mode, operand);
        cout << cs::pad(r.mode_name, 18) << cs::pad(r.operand_text, 10) << cs::pad(r.effective_address, 12)
             << cs::pad(std::to_string(r.value), 6) << r.memory_accesses << "회\n";
    }
    cout << "\n=> 즉시/레지스터 방식은 메모리에 가지 않아 빠르고, 간접 방식은 메모리를 두 번 가서 느리다.\n";

    // C++ 코드로 보면
    int data = 42;
    int* pointer = &data;
    int** pointer_to_pointer = &pointer;
    cout << "\nC++로 비유하면:\n"
         << "  int data = 42;            // 즉시값 42\n"
         << "  *pointer = " << *pointer << "            // 주소를 한 번 따라감 (직접/레지스터 간접)\n"
         << "  **pointer_to_pointer = " << **pointer_to_pointer << " // 주소의 주소를 따라감 (간접)\n";
}

int main() {
    cs::init_console();
    cs::title("컴퓨터 구조 - 명령어");
    instruction_encoding();
    addressing_modes();
    return 0;
}

// 직접 해보기
//  - 3-주소 명령어 [op 4비트][a 4비트][b 4비트][c 4비트] 인코더/디코더 만들기
//  - Mode::Indirect를 두 단계 간접(주소의 주소의 주소)으로 바꾸면 메모리 접근이 몇 번이 되는지 확인
