// [운영체제] 프로세스 동기화
// 블로그: https://bb0ng2.tistory.com/49
//
// 실습 내용 (프로세스 대신 스레드로 실습한다. 원리는 같다)
//   1. 경쟁 상태: 블로그의 은행 계좌 예제. balance = balance + 10000이 '읽기 -> 더하기 -> 쓰기'로 쪼개져서 생기는 문제
//   2. 뮤텍스 락으로 상호 배제
//   3. 락을 직접 만들어 보기: 원자적이지 않은 락은 실패하고, test-and-set으로 만든 스핀락은 성공 (바쁜 대기)
//   4. 세마포: 프린터 2대를 5개 작업이 나눠 쓰기 (카운팅 세마포)
//   5. 세마포로 실행 순서 제어: 블로그의 writer -> reader 예제
//   6. 모니터와 조건 변수: 생산자-소비자 (유한 버퍼)
#include "cs_common.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <semaphore>
#include <string>
#include <thread>
#include <vector>

using std::cout;
using namespace std::chrono_literals;

std::mutex g_print;  // 여러 스레드가 출력할 때 줄이 섞이지 않게 한다

template <typename... Args>
void say(Args&&... args) {
    std::lock_guard<std::mutex> lock(g_print);
    ((cout << std::forward<Args>(args)), ...);
    cout << "\n";
}

const int kRepeat = 100'000;

// ---------------------------------------------------------------
// 1. 경쟁 상태
// ---------------------------------------------------------------
void race_condition_demo() {
    cs::section("1. 경쟁 상태 (은행 계좌)");
    // 일반 int로 경쟁 상태를 만들면 C++에서는 정의되지 않은 동작이다.
    // 그래서 atomic의 load/store를 따로 호출해 CPU의 '읽기 -> 계산 -> 쓰기' 3단계를 그대로 흉내 낸다.
    std::atomic<long long> balance{0};
    auto deposit = [&] {
        for (int i = 0; i < kRepeat; ++i) {
            long long value = balance.load();  // 1) 메모리에서 읽기
            value += 10000;                    // 2) 레지스터에서 더하기
            balance.store(value);              // 3) 메모리에 쓰기  <- 1)~3) 사이에 다른 스레드가 끼어들 수 있다
        }
    };
    auto withdraw = [&] {
        for (int i = 0; i < kRepeat; ++i) {
            long long value = balance.load();
            value -= 10000;
            balance.store(value);
        }
    };
    for (int trial = 1; trial <= 3; ++trial) {
        balance = 0;
        std::thread a(deposit), b(withdraw);
        a.join();
        b.join();
        cout << "  시도 " << trial << ": 입금 " << kRepeat << "번, 출금 " << kRepeat << "번 후 잔액 = " << balance
             << "  (기대값 0)\n";
    }
}

// ---------------------------------------------------------------
// 2. 뮤텍스 락
// ---------------------------------------------------------------
void mutex_demo() {
    cs::section("2. 뮤텍스 락");
    long long balance = 0;
    std::mutex lock;
    auto work = [&](int amount) {
        for (int i = 0; i < kRepeat; ++i) {
            lock.lock();          // acquire()
            balance += amount;    // 임계 구역
            lock.unlock();        // release()
        }
    };
    std::thread a(work, 10000), b(work, -10000);
    a.join();
    b.join();
    cout << "  잔액 = " << balance << "  (항상 0)\n";
}

// ---------------------------------------------------------------
// 3. 락 직접 만들기
// ---------------------------------------------------------------
class NaiveLock {  // 블로그의 의사 코드 그대로: 확인과 설정이 따로 일어난다
public:
    void acquire() {
        while (locked_.load()) {}      // 잠겨 있으면 기다리다가
        std::this_thread::yield();     // (여기서 문맥 교환이 일어나면?)
        locked_.store(true);           // 잠근다
    }
    void release() { locked_.store(false); }

private:
    std::atomic<bool> locked_{false};
};

class SpinLock {  // test-and-set: 확인과 설정을 하나의 원자적 명령어로 한다
public:
    void acquire() {
        while (flag_.test_and_set()) ++spins;  // 바쁜 대기
    }
    void release() { flag_.clear(); }
    std::atomic<long long> spins{0};

private:
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};

template <typename Lock>
int count_violations(Lock& lock) {
    std::atomic<int> inside{0}, violations{0};
    auto work = [&] {
        for (int i = 0; i < 20'000; ++i) {
            lock.acquire();
            if (++inside > 1) ++violations;  // 임계 구역에 2개 이상 들어왔다 = 상호 배제 실패
            --inside;
            lock.release();
        }
    };
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) threads.emplace_back(work);
    for (auto& t : threads) t.join();
    return violations;
}

void lock_implementation_demo() {
    cs::section("3. 락 직접 만들기");
    NaiveLock naive;
    SpinLock spin;
    cout << "  확인과 설정이 분리된 락: 상호 배제 위반 " << count_violations(naive) << "회\n";
    cout << "  test-and-set 스핀락   : 상호 배제 위반 " << count_violations(spin) << "회, 바쁜 대기 반복 "
         << spin.spins << "회\n";
    cout << "  => 스핀락은 안전하지만 기다리는 동안 CPU를 계속 쓴다(바쁜 대기).\n";
}

// ---------------------------------------------------------------
// 4. 카운팅 세마포
// ---------------------------------------------------------------
void semaphore_demo() {
    cs::section("4. 세마포: 프린터 2대, 작업 5개");
    std::counting_semaphore<2> printers(2);  // S = 2
    std::atomic<int> using_now{0}, max_using{0};
    auto job = [&](int id) {
        printers.acquire();  // wait(): S가 0이면 대기 상태로, 아니면 S-- 후 진입
        int now = ++using_now;
        int expected = max_using.load();
        while (now > expected && !max_using.compare_exchange_weak(expected, now)) {}
        say("  작업 ", id, " 인쇄 시작 (사용 중인 프린터 ", now, "대)");
        std::this_thread::sleep_for(50ms);
        --using_now;
        say("  작업 ", id, " 인쇄 끝");
        printers.release();  // signal(): S++, 기다리던 작업을 깨운다
    };
    std::vector<std::thread> jobs;
    for (int i = 1; i <= 5; ++i) jobs.emplace_back(job, i);
    for (auto& t : jobs) t.join();
    cout << "  => 동시에 사용한 프린터는 최대 " << max_using << "대\n";
}

// ---------------------------------------------------------------
// 5. 세마포로 실행 순서 제어
// ---------------------------------------------------------------
void execution_order_demo() {
    cs::section("5. 세마포로 실행 순서 제어 (writer -> reader)");
    std::binary_semaphore written(0);  // S = 0에서 시작
    std::string book;

    std::thread reader([&] {
        say("  reader: 먼저 실행됐지만 wait()에서 기다린다");
        written.acquire();  // wait()
        say("  reader: book = \"", book, "\" 읽기 성공");
    });
    std::this_thread::sleep_for(100ms);  // 일부러 reader가 먼저 시작하게 한다
    std::thread writer([&] {
        book = "운영체제 공부";
        say("  writer: book.txt에 쓰기 완료 -> signal()");
        written.release();  // signal()
    });
    reader.join();
    writer.join();
}

// ---------------------------------------------------------------
// 6. 모니터: 생산자-소비자
// ---------------------------------------------------------------
class BoundedBuffer {  // 공유 데이터와 그 데이터를 다루는 연산을 하나로 묶는다 (Java의 synchronized와 비슷)
public:
    explicit BoundedBuffer(size_t capacity) : capacity_(capacity) {}

    void put(int item) {
        std::unique_lock<std::mutex> lock(monitor_);           // 모니터 진입 (한 번에 하나만)
        not_full_.wait(lock, [&] { return items_.size() < capacity_; });  // 가득 차면 조건 변수에서 대기
        items_.push(item);
        say("  생산 ", item, "  버퍼 [", items_.size(), "/", capacity_, "]");
        not_empty_.notify_one();                               // 기다리는 소비자 깨우기
    }

    int take() {
        std::unique_lock<std::mutex> lock(monitor_);
        not_empty_.wait(lock, [&] { return !items_.empty(); });  // 비어 있으면 대기
        int item = items_.front();
        items_.pop();
        say("            소비 ", item, "  버퍼 [", items_.size(), "/", capacity_, "]");
        not_full_.notify_one();
        return item;
    }

private:
    std::mutex monitor_;
    std::condition_variable not_full_, not_empty_;
    std::queue<int> items_;
    size_t capacity_;
};

void monitor_demo() {
    cs::section("6. 모니터와 조건 변수 (생산자-소비자, 버퍼 크기 3)");
    BoundedBuffer buffer(3);
    std::thread producer([&] {
        for (int i = 1; i <= 8; ++i) buffer.put(i);  // 생산은 빠르다
    });
    std::thread consumer([&] {
        for (int i = 1; i <= 8; ++i) {
            std::this_thread::sleep_for(30ms);  // 소비는 느리다 -> 버퍼가 차면 생산자가 기다린다
            buffer.take();
        }
    });
    producer.join();
    consumer.join();
}

int main() {
    cs::init_console();
    cs::title("운영체제 - 프로세스 동기화");
    race_condition_demo();
    mutex_demo();
    lock_implementation_demo();
    semaphore_demo();
    execution_order_demo();
    monitor_demo();
    return 0;
}

// 직접 해보기
//  - mutex_demo에서 lock()/unlock() 대신 std::lock_guard를 써 보고, 중간에 return해도 안전한 이유 생각하기
//  - semaphore_demo의 counting_semaphore<2>(2)를 (1)로 바꾸면 무엇과 같아지는지 확인하기
//  - BoundedBuffer의 wait 조건(람다)을 지우면 어떤 문제가 생길 수 있는지 생각하기 (허위 깨움)
