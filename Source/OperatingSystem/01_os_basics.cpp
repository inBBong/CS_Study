// [운영체제] 운영체제 시작하기
// 블로그: https://bb0ng2.tistory.com/45
//
// 실습 내용
//   1. 이중 모드: 사용자 모드에서는 하드웨어에 직접 접근할 수 없고, 시스템 호출로 커널에 요청한다.
//      시스템 호출 -> 소프트웨어 인터럽트 -> 작업 백업 -> 커널 모드 -> 커널 코드 실행 -> 사용자 모드 복귀
//   2. 실제 시스템 호출의 비용: 1바이트씩 매번 운영체제에 쓰기 요청 vs 버퍼에 모아서 한 번에 요청
//   3. 운영체제가 관리하는 자원 정보 엿보기 (PID, 논리 프로세서 수, 파일 시스템)
#include "cs_common.h"

#include <cstdio>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <map>
#include <stdexcept>
#include <string>
#include <thread>

#ifdef _WIN32
#include <process.h>
#define GET_PID _getpid
#else
#include <unistd.h>
#define GET_PID getpid
#endif

using std::cout;

// ---------------------------------------------------------------
// 1. 이중 모드와 시스템 호출 시뮬레이션
// ---------------------------------------------------------------
class SimulatedOS {
public:
    enum class Mode { User, Kernel };

    SimulatedOS() {
        // 시스템 호출 테이블: 번호 -> 커널 함수
        syscall_table_[1] = [this](const std::string& arg) { return kernel_write_disk(arg); };
        syscall_table_[2] = [this](const std::string&) { return std::string("disk = \"") + disk_ + "\""; };
        syscall_table_[3] = [](const std::string&) { return std::string("pid = 1234"); };
    }

    // 사용자 프로그램이 하드웨어에 직접 접근하려고 하면?
    void direct_hardware_access(const std::string& data) {
        if (mode_ == Mode::User) {
            throw std::runtime_error("보호 예외: 사용자 모드에서 하드웨어 직접 접근 불가 (운영체제가 프로그램을 종료시킴)");
        }
        disk_ += data;
    }

    // 시스템 호출 = 소프트웨어 인터럽트
    std::string syscall(int number, const std::string& arg = "") {
        cout << "    1) 시스템 호출 " << number << "번 발생 (소프트웨어 인터럽트)\n";
        cout << "    2) 현재 작업 상태(PC, 레지스터) 백업\n";
        mode_ = Mode::Kernel;  // 슈퍼바이저 플래그 = 1
        ++mode_switches_;
        cout << "    3) 커널 모드로 전환 (슈퍼바이저 플래그 = 1)\n";
        std::string result = syscall_table_.at(number)(arg);
        cout << "    4) 커널 코드 실행 -> " << result << "\n";
        mode_ = Mode::User;
        ++mode_switches_;
        cout << "    5) 사용자 모드로 복귀, 백업한 작업 재개\n";
        return result;
    }

    int mode_switches() const { return mode_switches_; }

private:
    Mode mode_ = Mode::User;
    int mode_switches_ = 0;
    std::string disk_;
    std::map<int, std::function<std::string(const std::string&)>> syscall_table_;

    std::string kernel_write_disk(const std::string& data) {
        direct_hardware_access(data);  // 커널 모드에서는 허용된다
        return "디스크에 \"" + data + "\" 기록 완료";
    }
};

void dual_mode_demo() {
    cs::section("1. 이중 모드와 시스템 호출");
    SimulatedOS os;

    cout << "  [사용자 프로그램] 디스크에 직접 쓰기 시도\n";
    try {
        os.direct_hardware_access("hello");
    } catch (const std::exception& e) {
        cout << "    -> " << e.what() << "\n";
    }

    cout << "\n  [사용자 프로그램] write 시스템 호출로 요청\n";
    os.syscall(1, "hello");
    cout << "\n  [사용자 프로그램] read 시스템 호출로 확인\n";
    os.syscall(2);
    cout << "\n  모드 전환 횟수: " << os.mode_switches() << "번\n";
}

// ---------------------------------------------------------------
// 2. 실제 시스템 호출의 비용
// ---------------------------------------------------------------
void syscall_cost_demo() {
    cs::section("2. 실제 시스템 호출 비용 측정");
    namespace fs = std::filesystem;
    const fs::path path = fs::temp_directory_path() / "cs_study_syscall_test.txt";
    const int bytes = 200'000;

    auto write_file = [&](bool buffered) {
        std::FILE* file = std::fopen(path.string().c_str(), "wb");
        if (!file) throw std::runtime_error("파일 열기 실패");
        // 버퍼 없음: fputc 한 번마다 운영체제에 쓰기를 요청한다 (WriteFile / write 시스템 호출)
        // 버퍼 있음: 64KB를 모았다가 한 번에 요청한다
        std::setvbuf(file, nullptr, buffered ? _IOFBF : _IONBF, 64 * 1024);
        cs::Stopwatch sw;
        for (int i = 0; i < bytes; ++i) std::fputc('a' + i % 26, file);
        std::fclose(file);
        return sw.ms();
    };

    double unbuffered = write_file(false);
    double buffered = write_file(true);
    cout << std::fixed << std::setprecision(1);
    cout << "  " << bytes << "바이트 쓰기\n";
    cout << "  " << cs::pad("1바이트마다 시스템 호출 (" + std::to_string(bytes) + "번)", 36) << std::setw(8) << unbuffered
         << " ms\n";
    cout << "  " << cs::pad("64KB씩 모아서 호출 (" + std::to_string((bytes + 65535) / 65536) + "번)", 36) << std::setw(8)
         << buffered << " ms\n";
    cout << "=> 사용자 모드 <-> 커널 모드 전환에는 비용이 든다. 그래서 표준 라이브러리는 버퍼를 쓴다.\n";
    fs::remove(path);
}

// ---------------------------------------------------------------
// 3. 운영체제가 관리하는 자원
// ---------------------------------------------------------------
void resources_demo() {
    cs::section("3. 운영체제가 관리하는 자원");
    namespace fs = std::filesystem;
    cout << "  프로세스 관리   : 이 프로그램의 PID = " << GET_PID() << "\n";
    cout << "  CPU 자원 관리   : 논리 프로세서 " << std::thread::hardware_concurrency() << "개를 여러 프로세스가 나눠 씀\n";
    cout << "  파일 시스템 관리: 현재 디렉터리 = " << fs::current_path().string() << "\n";
    auto space = fs::space(fs::current_path());
    cout << "                    디스크 여유 공간 = " << space.available / (1024 * 1024 * 1024) << " GB / 전체 "
         << space.capacity / (1024 * 1024 * 1024) << " GB\n";
    cout << "  => 위 정보는 모두 프로그램이 직접 하드웨어를 읽은 게 아니라 시스템 호출로 운영체제에 물어본 것이다.\n";
}

int main() {
    cs::init_console();
    cs::title("운영체제 - 운영체제 시작하기");
    dual_mode_demo();
    syscall_cost_demo();
    resources_demo();
    return 0;
}

// 직접 해보기
//  - Windows: Process Monitor(Sysinternals)로 이 프로그램이 호출하는 WriteFile 횟수 확인하기
//  - Linux: strace -c ./OS_01_os_basics 로 시스템 호출 종류와 횟수 세어 보기
//  - SimulatedOS에 시스템 호출 4번(메모리 할당)을 추가해 보기
