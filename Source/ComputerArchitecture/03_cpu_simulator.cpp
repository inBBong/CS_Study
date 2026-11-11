// [컴퓨터 구조] CPU의 작동 원리
// 블로그: https://bb0ng2.tistory.com/33
//
// 8비트 장난감 CPU를 만들어 명령어가 실행되는 과정을 한 줄씩 추적한다.
//
// 실습 내용
//   1. 레지스터: PC, IR, MAR, MBR, 범용 레지스터 R0~R3, 스택 포인터 SP, 플래그 레지스터
//   2. 인출 사이클: MAR <- PC, MBR <- M[MAR], IR <- MBR, PC <- PC + 1
//   3. ALU와 플래그: 제로(Z), 부호(S), 캐리(C), 오버플로(V), 인터럽트(I)
//   4. CALL / RET: 돌아올 주소를 스택에 저장하고 복귀하기
//   5. 하드웨어 인터럽트: 인터럽트 플래그로 막기(CLI/STI), 현재 상태 백업, 인터럽트 벡터로 ISR 찾기, IRET로 복귀
//
// 메모리 배치 (256 워드)
//   0x00 ~       : 프로그램 (코드)
//   0x64 (100)   : 결과 데이터
//   0xF0 미만    : 스택 (0xF0에서 아래로 자란다)
//   0xF8 ~ 0xFF  : 인터럽트 벡터 테이블 (IRQ 번호 -> ISR 시작 주소)
#include "cs_common.h"

#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

using std::cout;

enum class Op : uint8_t { HALT, LOADI, LOAD, STORE, ADD, SUB, JMP, JZ, JNZ, PUSH, POP, CALL, RET, PRINT, CLI, STI, IRET };

// 명령어 한 워드 = [연산 코드 8비트][오퍼랜드 a 8비트][오퍼랜드 b 8비트]
constexpr int32_t make(Op op, int a = 0, int b = 0) {
    return (static_cast<int32_t>(op) << 16) | ((a & 0xFF) << 8) | (b & 0xFF);
}

constexpr int kVectorTable = 0xF8;
constexpr int kStackTop = 0xF0;

struct Flags {
    bool Z = false;  // 제로
    bool S = false;  // 부호
    bool C = false;  // 캐리
    bool V = false;  // 오버플로
    bool I = true;   // 인터럽트 허용

    int32_t pack() const { return int(Z) | int(S) << 1 | int(C) << 2 | int(V) << 3 | int(I) << 4; }
    void unpack(int32_t v) { Z = v & 1, S = v & 2, C = v & 4, V = v & 8, I = v & 16; }
};

std::string disassemble(int32_t word) {
    Op op = static_cast<Op>((word >> 16) & 0xFF);
    int a = (word >> 8) & 0xFF;
    int b = word & 0xFF;
    std::string R = "R" + std::to_string(a);
    switch (op) {
        case Op::HALT: return "HALT";
        case Op::LOADI: return "LOADI " + R + ", #" + std::to_string(b);
        case Op::LOAD: return "LOAD " + R + ", [" + std::to_string(b) + "]";
        case Op::STORE: return "STORE " + R + ", [" + std::to_string(b) + "]";
        case Op::ADD: return "ADD " + R + ", R" + std::to_string(b);
        case Op::SUB: return "SUB " + R + ", R" + std::to_string(b);
        case Op::JMP: return "JMP " + std::to_string(b);
        case Op::JZ: return "JZ " + std::to_string(b);
        case Op::JNZ: return "JNZ " + std::to_string(b);
        case Op::PUSH: return "PUSH " + R;
        case Op::POP: return "POP " + R;
        case Op::CALL: return "CALL " + std::to_string(b);
        case Op::RET: return "RET";
        case Op::PRINT: return "PRINT " + R;
        case Op::CLI: return "CLI";
        case Op::STI: return "STI";
        case Op::IRET: return "IRET";
    }
    return "???";
}

class CPU {
public:
    std::array<int32_t, 256> mem{};
    std::array<uint8_t, 4> R{};
    uint8_t PC = 0, MAR = 0, SP = kStackTop;
    int32_t IR = 0, MBR = 0;
    Flags F;
    bool halted = false;
    int cycle = 0;

    // 입출력장치가 인터럽트 요청 신호를 보낸다.
    void request_interrupt(int irq) {
        irq_pending_ = true;
        irq_number_ = irq;
        cout << "        ** 장치가 IRQ " << irq << " 인터럽트 요청 신호를 보냄\n";
    }

    void step() {
        ++cycle;
        std::ostringstream log;
        uint8_t fetched_at = PC;

        // ---- 인출 사이클 ----
        MAR = PC;           // 1) PC의 주소를 MAR로
        MBR = mem[MAR];     // 2) 주소 버스로 주소 전달, 메모리 읽기 제어 신호 -> 데이터 버스로 MBR에 도착
        IR = MBR;           // 3) MBR의 명령어를 IR로
        PC = PC + 1;        // 4) PC 증가

        // ---- 실행 사이클 ----
        Op op = static_cast<Op>((IR >> 16) & 0xFF);
        int a = (IR >> 8) & 0xFF;
        int b = IR & 0xFF;
        switch (op) {
            case Op::HALT: halted = true; log << "정지"; break;
            case Op::LOADI: R[a] = static_cast<uint8_t>(b); log << "R" << a << " <- " << b; break;
            case Op::LOAD: R[a] = static_cast<uint8_t>(mem_read(b)); log << "R" << a << " <- M[" << b << "]"; break;
            case Op::STORE: mem_write(b, R[a]); log << "M[" << b << "] <- R" << a << "(" << int(R[a]) << ")"; break;
            case Op::ADD:
            case Op::SUB: {
                uint8_t x = R[a], y = R[b];  // 피연산자를 먼저 읽어 둔다 (ADD R3, R3처럼 같은 레지스터일 수 있음)
                R[a] = alu(op, x, y);
                log << "ALU: " << int(x) << (op == Op::ADD ? " + " : " - ") << int(y) << " = " << int(R[a]) << " -> R"
                    << a;
                break;
            }
            case Op::JMP: PC = static_cast<uint8_t>(b); log << "PC <- " << b; break;
            case Op::JZ: if (F.Z) PC = static_cast<uint8_t>(b); log << (F.Z ? "Z=1, 분기" : "Z=0, 분기 안 함"); break;
            case Op::JNZ: if (!F.Z) PC = static_cast<uint8_t>(b); log << (!F.Z ? "Z=0, 분기" : "Z=1, 분기 안 함"); break;
            case Op::PUSH: push(R[a]); log << "스택에 R" << a << " 저장, SP=" << int(SP); break;
            case Op::POP: R[a] = static_cast<uint8_t>(pop()); log << "스택에서 R" << a << " 복원, SP=" << int(SP); break;
            case Op::CALL: push(PC); log << "복귀 주소(" << int(PC) << ")를 스택에 저장, PC <- " << b; PC = static_cast<uint8_t>(b); break;
            case Op::RET: PC = static_cast<uint8_t>(pop()); log << "스택에서 복귀 주소(" << int(PC) << ") 꺼냄"; break;
            case Op::PRINT: log << "출력장치로 전송 -> [출력장치] R" << a << " = " << int(R[a]); break;
            case Op::CLI: F.I = false; log << "인터럽트 플래그 = 0 (막음)"; break;
            case Op::STI: F.I = true; log << "인터럽트 플래그 = 1 (허용)"; break;
            case Op::IRET:
                F.unpack(pop());
                PC = static_cast<uint8_t>(pop());
                log << "백업한 플래그와 PC(" << int(PC) << ") 복구, 원래 작업으로 복귀";
                break;
        }

        cout << "[" << std::setw(2) << cycle << "] " << std::setw(3) << int(fetched_at) << ": " << std::left
             << std::setw(15) << disassemble(IR) << std::right << " | " << log.str() << "\n";
        cout << "        R=[" << int(R[0]) << "," << int(R[1]) << "," << int(R[2]) << "," << int(R[3]) << "] PC=" << int(PC)
             << " SP=" << int(SP) << "  Z" << F.Z << " S" << F.S << " C" << F.C << " V" << F.V << " I" << F.I << "\n";

        // ---- 인터럽트 사이클: 실행이 끝나고 다음 인출 전에 확인 ----
        check_interrupt();
    }

private:
    bool irq_pending_ = false;
    int irq_number_ = 0;
    bool reported_masked_ = false;

    int32_t mem_read(int address) {
        MAR = static_cast<uint8_t>(address);
        MBR = mem[MAR];
        return MBR;
    }
    void mem_write(int address, int32_t value) {
        MAR = static_cast<uint8_t>(address);
        MBR = value;
        mem[MAR] = MBR;
    }
    void push(int32_t value) { mem_write(--SP, value); }
    int32_t pop() { return mem_read(SP++); }

    // 8비트 ALU: 결과와 함께 플래그를 만든다.
    uint8_t alu(Op op, uint8_t x, uint8_t y) {
        int wide = (op == Op::ADD) ? int(x) + int(y) : int(x) - int(y);
        uint8_t r = static_cast<uint8_t>(wide);
        F.Z = (r == 0);
        F.S = (r & 0x80) != 0;
        F.C = (op == Op::ADD) ? wide > 0xFF : wide < 0;  // 올림 / 빌림
        // 부호 있는 수로 봤을 때 범위(-128~127)를 넘었는지
        F.V = (op == Op::ADD) ? ((~(x ^ y) & (x ^ r) & 0x80) != 0) : (((x ^ y) & (x ^ r) & 0x80) != 0);
        return r;
    }

    void check_interrupt() {
        if (!irq_pending_ || halted) return;
        if (!F.I) {
            if (!reported_masked_) cout << "        ** 인터럽트 요청이 있지만 인터럽트 플래그가 0이라 대기\n";
            reported_masked_ = true;
            return;
        }
        irq_pending_ = false;
        reported_masked_ = false;
        cout << "        ** 인터럽트 처리 시작: PC(" << int(PC) << ")와 플래그를 스택에 백업\n";
        push(PC);
        push(F.pack());
        F.I = false;  // ISR 실행 중에는 다른 인터럽트를 막는다
        int isr = mem[kVectorTable + irq_number_];
        cout << "        ** 인터럽트 벡터 M[" << kVectorTable + irq_number_ << "] -> ISR 시작 주소 " << isr << "\n";
        PC = static_cast<uint8_t>(isr);
    }
};

int main() {
    cs::init_console();
    cs::title("컴퓨터 구조 - CPU의 작동 원리 (8비트 CPU 시뮬레이터)");

    CPU cpu;
    // 메인 프로그램: 1 + 2 + 3 + 4 + 5를 계산해서 100번지에 저장하고 출력한다.
    cpu.mem[0] = make(Op::CLI);            // 초기화 동안 인터럽트를 막는다
    cpu.mem[1] = make(Op::LOADI, 0, 0);    // R0 = 합계
    cpu.mem[2] = make(Op::LOADI, 1, 5);    // R1 = 카운터
    cpu.mem[3] = make(Op::LOADI, 2, 1);    // R2 = 1
    cpu.mem[4] = make(Op::STI);            // 이제 인터럽트 허용
    cpu.mem[5] = make(Op::ADD, 0, 1);      // loop: R0 += R1
    cpu.mem[6] = make(Op::SUB, 1, 2);      //       R1 -= 1  (0이 되면 Z=1)
    cpu.mem[7] = make(Op::JNZ, 0, 5);      //       Z=0이면 loop로
    cpu.mem[8] = make(Op::STORE, 0, 100);  // M[100] = R0
    cpu.mem[9] = make(Op::CALL, 0, 20);    // 플래그 실험 서브루틴 호출
    cpu.mem[10] = make(Op::PRINT, 0);
    cpu.mem[11] = make(Op::HALT);

    // 서브루틴: 8비트 덧셈의 캐리/오버플로 플래그 관찰
    cpu.mem[20] = make(Op::LOADI, 3, 100);
    cpu.mem[21] = make(Op::ADD, 3, 3);     // 100 + 100 = 200: 부호 있는 범위 초과 -> V=1
    cpu.mem[22] = make(Op::LOADI, 3, 200);
    cpu.mem[23] = make(Op::ADD, 3, 3);     // 200 + 200 = 400: 8비트 초과 -> C=1, 결과 144
    cpu.mem[24] = make(Op::RET);

    // 키보드 인터럽트 서비스 루틴 (ISR)
    cpu.mem[40] = make(Op::PUSH, 3);       // 사용할 레지스터를 먼저 백업
    cpu.mem[41] = make(Op::LOADI, 3, 'K');
    cpu.mem[42] = make(Op::PRINT, 3);      // 키보드에서 받은 값 'K'(75) 처리
    cpu.mem[43] = make(Op::POP, 3);
    cpu.mem[44] = make(Op::IRET);

    cpu.mem[kVectorTable + 1] = 40;  // IRQ 1(키보드)의 ISR은 40번지

    cout << "[사이클] 주소: 명령어        | 실행 내용\n";
    while (!cpu.halted && cpu.cycle < 100) {
        if (cpu.cycle == 2) cpu.request_interrupt(1);  // 3번째 명령어 실행 전에 키보드가 인터럽트 요청
        cpu.step();
    }
    cout << "\n프로그램 종료: M[100] = " << cpu.mem[100] << " (1+2+3+4+5)\n";
    return 0;
}

// 직접 해보기
//  - 인터럽트 요청 시점을 cycle == 7로 바꾸면 어디서 ISR로 들어가는지 확인하기
//  - mem[40]의 PUSH R3를 빼면 ISR 이후 R3 값이 어떻게 망가지는지 확인하기
//  - MUL 명령어를 추가해서 5! 계산 프로그램 작성하기
