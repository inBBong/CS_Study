// [컴퓨터 구조] 입출력장치
// 블로그: https://bb0ng2.tistory.com/43
//
// 실제 하드웨어 대신 스레드 하나를 '느린 프린터'로 삼아 입출력 방식을 비교한다.
//
// 실습 내용
//   1. 장치 컨트롤러: 데이터 / 상태 / 제어 레지스터
//   2. 프로그램 입출력 + 폴링: CPU가 상태 레지스터를 계속 확인 -> 다른 일을 못 함
//   3. 인터럽트 기반 입출력: CPU는 다른 일을 하다가 인터럽트가 오면 ISR 실행
//   4. DMA: 큰 데이터를 DMA 컨트롤러가 메모리로 직접 옮기고, 시스템 버스는 사이클 스틸링으로 빌려 씀
//   5. 여러 인터럽트: 우선순위(PIC), 마스크 불가능 인터럽트(NMI)
//   6. 메모리 맵 입출력 vs 고립형 입출력
#include "cs_common.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using std::cout;
using namespace std::chrono_literals;

// ---------------------------------------------------------------
// 1. 장치 컨트롤러
// ---------------------------------------------------------------
enum Status { kIdle = 0, kBusy = 1, kDone = 2 };
enum Command { kNone = 0, kWrite = 1, kStop = 2 };

class PrinterController {
public:
    std::atomic<char> data_register{0};
    std::atomic<int> status_register{kIdle};
    std::atomic<int> control_register{kNone};
    std::atomic<bool>* interrupt_line = nullptr;  // 연결되어 있으면 작업 완료 시 인터럽트 요청
    std::atomic<size_t> printed_count{0};
    std::string printed;  // 장치 스레드만 쓴다. stop()으로 스레드가 끝난 뒤에 읽을 것

    void start() {
        worker_ = std::thread([this] { run(); });
    }
    void stop() {
        control_register = kStop;
        worker_.join();
    }

private:
    std::thread worker_;

    void run() {
        while (true) {
            int cmd = control_register.exchange(kNone);
            if (cmd == kStop) return;
            if (cmd == kWrite) {
                status_register = kBusy;
                std::this_thread::sleep_for(20ms);  // 프린터는 CPU보다 훨씬 느리다
                printed += data_register.load();
                ++printed_count;
                status_register = kDone;
                if (interrupt_line) *interrupt_line = true;
            }
            std::this_thread::yield();
        }
    }
};

// CPU가 하는 '다른 유용한 일'을 흉내 낸다.
long long useful_work(long long seed) {
    return (seed * 2654435761LL) % 1000003;
}

// ---------------------------------------------------------------
// 2. 프로그램 입출력 + 폴링
// ---------------------------------------------------------------
void polling_demo(const std::string& text) {
    cs::section("2. 프로그램 입출력 + 폴링");
    PrinterController printer;
    printer.start();
    long long polls = 0;
    cs::Stopwatch sw;
    for (char c : text) {
        printer.data_register = c;         // CPU가 데이터 레지스터에 쓰고
        printer.control_register = kWrite;  // 제어 레지스터에 명령
        while (printer.status_register != kDone || printer.control_register != kNone) ++polls;  // 끝날 때까지 계속 확인
        printer.status_register = kIdle;
    }
    double ms = sw.ms();
    printer.stop();
    cout << "  출력 결과: \"" << printer.printed << "\" (" << std::fixed << std::setprecision(0) << ms << " ms)\n";
    cout << "  상태 레지스터 확인 횟수: " << polls << "회\n";
    cout << "  그동안 CPU가 한 다른 일: 0건\n";
}

// ---------------------------------------------------------------
// 3. 인터럽트 기반 입출력
// ---------------------------------------------------------------
void interrupt_demo(const std::string& text) {
    cs::section("3. 인터럽트 기반 입출력");
    std::atomic<bool> irq{false};
    PrinterController printer;
    printer.interrupt_line = &irq;
    printer.start();

    size_t next = 0;
    long long work_done = 0, checksum = 0;
    // ISR: 다음 글자를 보낸다
    auto isr = [&] {
        printer.status_register = kIdle;
        if (next < text.size()) {
            printer.data_register = text[next++];
            printer.control_register = kWrite;
        }
    };

    cs::Stopwatch sw;
    isr();  // 첫 글자는 직접 보낸다
    while (printer.printed_count < text.size()) {
        checksum += useful_work(work_done++);  // 명령어 하나 실행
        if (irq.exchange(false)) isr();        // 명령어 실행이 끝날 때마다 인터럽트 확인
    }
    double ms = sw.ms();
    printer.stop();
    cout << "  출력 결과: \"" << printer.printed << "\" (" << std::fixed << std::setprecision(0) << ms << " ms)\n";
    cout << "  그동안 CPU가 한 다른 일: " << work_done << "건  (checksum " << checksum % 100 << ")\n";
    cout << "=> 걸린 시간은 비슷하지만, 인터럽트 방식은 기다리는 동안 CPU가 다른 일을 했다.\n";
}

// ---------------------------------------------------------------
// 4. DMA
// ---------------------------------------------------------------
void dma_demo() {
    cs::section("4. DMA 입출력과 사이클 스틸링");
    std::vector<char> memory(8 * 1024 * 1024, 'M');  // 메모리의 8MB 데이터를
    std::vector<char> device_buffer(memory.size());  // 장치(예: 디스크)로 보낸다
    std::mutex system_bus;                           // CPU와 DMA 컨트롤러가 함께 쓰는 시스템 버스
    std::atomic<bool> dma_done_irq{false};
    std::atomic<int> cycles_stolen{0};

    cout << "  CPU -> DMA 컨트롤러: \"메모리 0번지부터 8MB를 장치에 써라\" (CPU 관여 1)\n";
    std::thread dma_controller([&] {
        const size_t chunk = 4096;
        for (size_t offset = 0; offset < memory.size(); offset += chunk) {
            std::lock_guard<std::mutex> bus(system_bus);  // 버스를 잠깐 빌린다
            std::memcpy(&device_buffer[offset], &memory[offset], chunk);
            ++cycles_stolen;
        }
        dma_done_irq = true;  // 끝나면 인터럽트로 알린다
    });

    long long work_done = 0, checksum = 0;
    while (!dma_done_irq) {
        checksum += useful_work(work_done++);
        if (work_done % 256 == 0) {
            std::lock_guard<std::mutex> bus(system_bus);  // CPU도 가끔 메모리에 접근하려면 버스가 필요
            checksum += memory[static_cast<size_t>(work_done) % memory.size()];
        }
    }
    dma_controller.join();
    cout << "  DMA 컨트롤러 -> CPU: 완료 인터럽트 (CPU 관여 2)\n";
    cout << "  전송 확인: " << (device_buffer == memory ? "성공" : "실패") << ", 버스를 빌린 횟수 " << cycles_stolen
         << "회\n";
    cout << "  전송하는 동안 CPU가 한 다른 일: " << work_done << "건 (checksum " << checksum % 100 << ")\n";
}

// ---------------------------------------------------------------
// 5. 여러 인터럽트와 우선순위
// ---------------------------------------------------------------
struct InterruptRequest {
    int time;
    std::string name;
    int priority;  // 작을수록 높은 우선순위
    int duration;
    bool maskable;
};

void priority_demo() {
    cs::section("5. 여러 인터럽트: 우선순위와 NMI");
    // 디스크 ISR이 실행 중일 때 키보드(낮음), 타이머(높음), 정전(NMI)이 연달아 들어온다.
    std::vector<InterruptRequest> requests = {
        {0, "디스크", 3, 4, true},
        {1, "키보드", 5, 2, true},
        {2, "타이머", 1, 2, true},
        {5, "정전(NMI)", 0, 1, false},
    };
    bool interrupt_flag = true;
    struct Running {
        InterruptRequest req;
        int remaining;
    };
    std::vector<Running> stack;               // 실행 중인 ISR (중첩되면 위로 쌓임)
    std::vector<InterruptRequest> pending;    // PIC가 들고 있는 대기 요청

    for (int t = 0; t < 14; ++t) {
        for (const auto& r : requests)
            if (r.time == t) {
                pending.push_back(r);
                cout << "  t=" << std::setw(2) << t << "  요청: " << r.name << " (우선순위 " << r.priority << ")\n";
            }
        if (t == 4) {
            interrupt_flag = false;
            cout << "  t= 4  CPU가 인터럽트 플래그를 0으로 설정 (마스크 가능 인터럽트 차단)\n";
        }
        if (t == 8) {
            interrupt_flag = true;
            cout << "  t= 8  인터럽트 플래그를 다시 1로 설정\n";
        }
        // PIC: 대기 중 가장 높은 우선순위를 고르고, 실행 중인 ISR보다 높으면 CPU에 전달
        auto best = pending.end();
        for (auto it = pending.begin(); it != pending.end(); ++it) {
            if (it->maskable && !interrupt_flag) continue;
            if (best == pending.end() || it->priority < best->priority) best = it;
        }
        if (best != pending.end() && (stack.empty() || best->priority < stack.back().req.priority)) {
            if (!stack.empty()) cout << "  t=" << std::setw(2) << t << "  " << stack.back().req.name << " ISR 중단 -> ";
            else cout << "  t=" << std::setw(2) << t << "  ";
            cout << best->name << " ISR 시작\n";
            stack.push_back({*best, best->duration});
            pending.erase(best);
        }
        if (!stack.empty() && --stack.back().remaining == 0) {
            cout << "  t=" << std::setw(2) << t + 1 << "  " << stack.back().req.name << " ISR 완료\n";
            stack.pop_back();
        }
    }
}

// ---------------------------------------------------------------
// 6. 메모리 맵 입출력 vs 고립형 입출력
// ---------------------------------------------------------------
class MemoryMappedBus {
public:
    // 주소 0~511은 메모리, 512~1023은 장치 컨트롤러의 레지스터
    void write(int address, int value) {
        if (address < 512) {
            memory_[address] = value;
            cout << "    [" << address << "] = " << value << "  -> 메모리에 저장\n";
        } else {
            cout << "    [" << address << "] = " << value << "  -> 장치 레지스터 " << address - 512 << "번에 전달\n";
        }
    }

private:
    int memory_[512]{};
};

void io_mapping_demo() {
    cs::section("6. 메모리 맵 입출력 vs 고립형 입출력");
    cout << "  메모리 맵: 같은 STORE 명령어가 주소에 따라 메모리 또는 장치로 간다\n";
    MemoryMappedBus bus;
    bus.write(100, 7);
    bus.write(600, 1);
    cout << "  고립형: 메모리 주소 공간은 0~1023 전부 메모리, 장치는 별도 명령어 IN/OUT과 포트 번호로 접근\n";
    cout << "    OUT 0x60, 1   (예: x86의 키보드 컨트롤러 포트 0x60)\n";
}

int main() {
    cs::init_console();
    cs::title("컴퓨터 구조 - 입출력장치");
    cs::section("1. 장치 컨트롤러");
    cout << "  PrinterController = 데이터 레지스터 + 상태 레지스터 + 제어 레지스터 (코드 참고)\n";
    const std::string text = "HELLO, CPU!";
    polling_demo(text);
    interrupt_demo(text);
    dma_demo();
    priority_demo();
    io_mapping_demo();
    return 0;
}

// 직접 해보기
//  - PrinterController의 sleep_for를 1ms / 100ms로 바꿔서 폴링 횟수와 다른 일의 양 비교하기
//  - DMA 청크 크기를 바꿔 '버스를 빌린 횟수'와 CPU 작업량이 어떻게 변하는지 보기
