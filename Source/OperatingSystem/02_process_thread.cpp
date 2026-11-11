// [운영체제] 프로세스와 스레드
// 블로그: https://bb0ng2.tistory.com/47
//
// 실습 내용
//   1. 프로세스의 메모리 영역: 코드 / 데이터 / 힙 / 스택의 실제 주소 출력, 스택이 자라는 방향 확인
//   2. PCB와 문맥 교환: 프로세스 상태 전이(생성 -> 준비 -> 실행 -> 대기 -> 종료)와 PC 저장/복구 시뮬레이션
//   3. 스레드: 같은 프로세스의 스레드는 코드/데이터/힙을 공유하고, 스택과 레지스터는 각자 가진다
//   4. 프로세스 생성: Unix 계열은 fork(), Windows는 자식 프로세스 실행으로 부모-자식 관계 확인
#include "cs_common.h"

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <process.h>
#define GET_PID _getpid
#else
#include <sys/wait.h>
#include <unistd.h>
#define GET_PID getpid
#endif

using std::cout;

// ---------------------------------------------------------------
// 1. 메모리 영역
// ---------------------------------------------------------------
int g_initialized = 100;  // 데이터 영역
int g_uninitialized;      // 데이터 영역 (BSS: 0으로 초기화되는 부분)
const char* g_message = "Hello";

void code_region_function() {}

void print_stack_growth(int depth) {
    int local = depth;  // 함수가 호출될 때마다 스택에 새로 쌓인다
    cout << "    깊이 " << depth << "의 지역 변수 주소: " << &local << "\n";
    if (depth < 3) print_stack_growth(depth + 1);
}

void memory_regions_demo() {
    cs::section("1. 프로세스의 메모리 영역");
    static int s_static = 1;                          // 데이터 영역
    auto heap = std::make_unique<int>(42);            // 힙 영역 (new로 동적 할당)
    int stack_local = 0;                              // 스택 영역

    struct Region {
        std::string name;
        const void* address;
    };
    std::vector<Region> regions = {
        {"코드   함수 code_region_function", reinterpret_cast<const void*>(&code_region_function)},
        {"코드   문자열 리터럴 \"Hello\"", g_message},
        {"데이터 전역 변수 g_initialized", &g_initialized},
        {"데이터 전역 변수 g_uninitialized", &g_uninitialized},
        {"데이터 static 변수 s_static", &s_static},
        {"힙     make_unique<int>", heap.get()},
        {"스택   지역 변수 stack_local", &stack_local},
    };
    std::sort(regions.begin(), regions.end(), [](const Region& a, const Region& b) { return a.address < b.address; });
    cout << "  주소가 낮은 순서:\n";
    for (const auto& r : regions) cout << "    " << r.address << "  " << r.name << "\n";
    cout << "  (Windows는 스택이 힙보다 낮은 주소에 놓이는 경우가 많다. 영역 배치는 운영체제마다 다르다)\n\n";

    cout << "  함수를 재귀 호출하면 스택은 낮은 주소 쪽으로 자란다:\n";
    print_stack_growth(1);

    cout << "\n  코드 영역은 읽기 전용이다. 아래 주석을 풀면 접근 위반으로 프로그램이 죽는다.\n";
    // char* p = const_cast<char*>(g_message); p[0] = 'J';
}

// ---------------------------------------------------------------
// 2. PCB와 문맥 교환
// ---------------------------------------------------------------
enum class State { New, Ready, Running, Blocked, Terminated };

const char* state_name(State s) {
    switch (s) {
        case State::New: return "생성";
        case State::Ready: return "준비";
        case State::Running: return "실행";
        case State::Blocked: return "대기";
        case State::Terminated: return "종료";
    }
    return "";
}

struct PCB {
    int pid;
    State state = State::New;
    int program_counter = 0;  // 어디까지 실행했는지
    int priority = 0;
};

class MiniScheduler {
public:
    void create(int pid) {
        processes_.push_back({pid});
        log("P" + std::to_string(pid) + " 생성 -> PCB 할당");
        set_state(pid, State::Ready);
    }

    void dispatch(int pid) {
        if (running_ != -1) {
            PCB& prev = find(running_);
            prev.program_counter = cpu_pc_;  // 문맥 저장: CPU의 PC를 PCB에 백업
            cout << "    문맥 저장: P" << prev.pid << "의 PC=" << cpu_pc_ << " -> PCB\n";
        }
        PCB& next = find(pid);
        cpu_pc_ = next.program_counter;  // 문맥 복구: PCB의 PC를 CPU로
        cout << "    문맥 복구: PCB -> P" << pid << "의 PC=" << cpu_pc_ << "\n";
        running_ = pid;
        set_state(pid, State::Running);
        log("디스패치: P" + std::to_string(pid) + " 실행");
    }

    void run(int instructions) {
        cpu_pc_ += instructions;
        log("P" + std::to_string(running_) + " 명령어 " + std::to_string(instructions) + "개 실행 (PC=" +
            std::to_string(cpu_pc_) + ")");
    }

    void timer_interrupt() {
        int pid = running_;
        save_running();
        set_state(pid, State::Ready);
        log("타이머 인터럽트: P" + std::to_string(pid) + " 실행 -> 준비");
    }

    void request_io() {
        int pid = running_;
        save_running();
        set_state(pid, State::Blocked);
        log("입출력 요청: P" + std::to_string(pid) + " 실행 -> 대기");
    }

    void io_complete(int pid) {
        set_state(pid, State::Ready);
        log("입출력 완료: P" + std::to_string(pid) + " 대기 -> 준비 (바로 실행으로 가지 않는다)");
    }

    void exit_running() {
        int pid = running_;
        set_state(pid, State::Terminated);
        running_ = -1;
        log("P" + std::to_string(pid) + " 종료 -> PCB와 메모리 정리");
    }

private:
    std::vector<PCB> processes_;
    int running_ = -1;
    int cpu_pc_ = 0;

    PCB& find(int pid) {
        return *std::find_if(processes_.begin(), processes_.end(), [pid](const PCB& p) { return p.pid == pid; });
    }
    void set_state(int pid, State s) { find(pid).state = s; }
    void save_running() {
        find(running_).program_counter = cpu_pc_;
        cout << "    문맥 저장: P" << running_ << "의 PC=" << cpu_pc_ << " -> PCB\n";
        running_ = -1;
    }
    void log(const std::string& event) {
        cout << "  " << cs::pad(event, 56) << "|";
        for (const auto& p : processes_) cout << " P" << p.pid << ":" << state_name(p.state);
        cout << std::right << "\n";
    }
};

void pcb_demo() {
    cs::section("2. PCB, 프로세스 상태, 문맥 교환");
    MiniScheduler os;
    os.create(1);
    os.create(2);
    os.dispatch(1);
    os.run(5);
    os.timer_interrupt();
    os.dispatch(2);
    os.run(3);
    os.request_io();
    os.dispatch(1);  // P1은 아까 멈춘 PC=5부터 이어서 실행된다
    os.run(4);
    os.io_complete(2);
    os.timer_interrupt();
    os.dispatch(2);
    os.run(2);
    os.exit_running();
    os.dispatch(1);
    os.exit_running();
}

// ---------------------------------------------------------------
// 3. 스레드: 공유하는 것과 따로 가지는 것
// ---------------------------------------------------------------
int g_shared_counter = 0;

void thread_demo() {
    cs::section("3. 스레드 (Hello, world 3번)");
    std::mutex print_mutex;
    auto heap_data = std::make_shared<int>(7);

    auto worker = [&](int number) {
        int my_stack_variable = number;  // 스레드마다 자기 스택에 따로 생긴다
        std::lock_guard<std::mutex> lock(print_mutex);
        ++g_shared_counter;
        std::ostringstream id;
        id << std::this_thread::get_id();
        cout << "  Hello, world  [스레드 ID " << std::setw(6) << id.str() << "]  PID " << GET_PID()
             << "  스택 변수 " << &my_stack_variable << "  전역 변수 " << &g_shared_counter << "  힙 "
             << heap_data.get() << "\n";
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 3; ++i) threads.emplace_back(worker, i);
    for (auto& t : threads) t.join();
    cout << "  => PID는 모두 같다(한 프로세스). 전역 변수와 힙 주소는 같고(공유), 스택 변수 주소는 다르다(각자).\n";
    cout << "     세 스레드가 함께 올린 전역 카운터 = " << g_shared_counter << "\n";
}

// ---------------------------------------------------------------
// 4. 프로세스 생성
// ---------------------------------------------------------------
void process_creation_demo(const char* self_path) {
    cs::section("4. 프로세스 생성 (부모와 자식)");
#if defined(_WIN32)
    cout << "  Windows에는 fork()가 없다. 대신 CreateProcess로 새 프로그램을 실행한다(fork + exec와 비슷).\n";
    cout << "  부모 PID = " << GET_PID() << ". 자식 프로세스를 실행한다.\n";
    std::string command = "\"\"" + std::string(self_path) + "\" --child " + std::to_string(GET_PID()) + "\"";
    std::cout.flush();             // 자식 출력과 섞이지 않게 먼저 비운다
    std::system(command.c_str());  // 내부적으로 CreateProcess
    cout << "  부모: 자식이 끝날 때까지 기다렸다가 계속 실행\n";
#else
    (void)self_path;
    cout << "  fork()를 두 번 연속 호출하면 프로세스는 몇 개가 될까?\n";
    std::cout.flush();  // fork 전에 버퍼를 비우지 않으면 출력이 복제된다
    pid_t first = fork();
    pid_t second = fork();  // 부모와 첫 번째 자식 모두 이 줄을 실행한다
    std::printf("    Hello, world  PID=%d  PPID=%d\n", getpid(), getppid());
    std::fflush(stdout);
    if (second > 0) waitpid(second, nullptr, 0);
    if (first > 0) waitpid(first, nullptr, 0);
    if (first == 0 || second == 0) std::exit(0);  // 자식들은 여기서 끝낸다
    cout << "  => Hello, world가 4번 출력된다 (블로그의 fork 함정). 정확히 3개를 원하면 부모만 fork해야 한다.\n";
#endif
}

int main(int argc, char* argv[]) {
    cs::init_console();
    if (argc >= 3 && std::string(argv[1]) == "--child") {
        cout << "    [자식] 나의 PID = " << GET_PID() << ", 부모 PID(전달받음) = " << argv[2] << "\n";
        return 0;
    }
    cs::title("운영체제 - 프로세스와 스레드");
    memory_regions_demo();
    pcb_demo();
    thread_demo();
    process_creation_demo(argv[0]);
    return 0;
}

// 직접 해보기
//  - 작업 관리자 > 세부 정보 탭에서 PID 열을 확인하고, 이 프로그램 실행 중의 PID와 비교하기
//  - thread_demo에서 lock_guard를 지우면 출력이 섞이는지 관찰하기
//  - MiniScheduler에 우선순위를 추가해서 dispatch할 프로세스를 자동으로 고르게 만들기
