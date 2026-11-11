// [컴퓨터 구조] CPU 성능 향상 기법
// 블로그: https://bb0ng2.tistory.com/34
//
// 실습 내용
//   1. 명령어 파이프라인 시뮬레이션: 파이프라인 없음 vs 이상적 파이프라인 vs 데이터 위험(hazard) 발생
//   2. 비순차 명령어 처리(OoOE): 의존성 없는 명령어를 먼저 실행해 빈 사이클을 메우기
//   3. 분기 예측: 정렬된 데이터와 무작위 데이터에서 같은 if문의 속도 차이
//   4. 멀티코어 / 멀티스레드: 스레드 수에 따른 속도, 작업을 고르게 나누는 것의 중요성
#include "cs_common.h"

#include <algorithm>
#include <atomic>
#include <iomanip>
#include <random>
#include <string>
#include <thread>
#include <vector>

using std::cout;

// ---------------------------------------------------------------
// 1, 2. 파이프라인
// ---------------------------------------------------------------
struct PipeInstr {
    std::string text;
    int dst;               // 쓰는 레지스터
    std::vector<int> src;  // 읽는 레지스터
};

bool depends_raw(const PipeInstr& earlier, const PipeInstr& later) {  // later가 earlier의 결과를 읽음
    return std::find(later.src.begin(), later.src.end(), earlier.dst) != later.src.end();
}
bool conflicts(const PipeInstr& earlier, const PipeInstr& later) {  // 순서를 바꾸면 결과가 달라지는 관계
    return depends_raw(earlier, later) || earlier.dst == later.dst ||
           std::find(earlier.src.begin(), earlier.src.end(), later.dst) != earlier.src.end();
}

// 단계: IF(인출) ID(해석) EX(실행) WB(저장). start[i] = i번 명령어가 IF를 시작하는 사이클
// 데이터 위험: 뒤 명령어의 ID는 앞 명령어의 WB 이후여야 한다 -> start[later] >= start[earlier] + 2
std::vector<int> schedule_in_order(const std::vector<PipeInstr>& prog) {
    std::vector<int> start(prog.size());
    for (size_t i = 0; i < prog.size(); ++i) {
        int t = (i == 0) ? 0 : start[i - 1] + 1;
        for (size_t j = 0; j < i; ++j) {
            if (depends_raw(prog[j], prog[i])) t = std::max(t, start[j] + 2);
        }
        start[i] = t;
    }
    return start;
}

// 매 사이클, 아직 실행 안 한 명령어 중 지금 바로 실행 가능한 가장 앞의 것을 고른다.
std::vector<int> schedule_out_of_order(const std::vector<PipeInstr>& prog) {
    std::vector<int> start(prog.size(), -1);
    size_t issued = 0;
    for (int cycle = 0; issued < prog.size(); ++cycle) {
        for (size_t i = 0; i < prog.size(); ++i) {
            if (start[i] != -1) continue;
            bool ready = true;
            for (size_t j = 0; j < i && ready; ++j) {
                if (!conflicts(prog[j], prog[i])) continue;
                if (start[j] == -1) ready = false;                                    // 앞의 관련 명령어가 아직 안 나감
                else if (depends_raw(prog[j], prog[i]) && cycle < start[j] + 2) ready = false;  // 결과가 아직 없음
            }
            if (ready) {
                start[i] = cycle;
                ++issued;
                break;  // 한 사이클에 하나씩만 인출
            }
        }
    }
    return start;
}

void print_pipeline(const std::vector<PipeInstr>& prog, const std::vector<int>& start) {
    int total = 0;
    for (int s : start) total = std::max(total, s + 4);
    const char* stages[] = {"IF", "ID", "EX", "WB"};
    cout << cs::pad("사이클", 16);
    for (int c = 1; c <= total; ++c) cout << std::setw(3) << c;
    cout << "\n";
    for (size_t i = 0; i < prog.size(); ++i) {
        cout << std::left << std::setw(16) << prog[i].text << std::right;
        for (int c = 0; c < total; ++c) {
            int stage = c - start[i];
            cout << std::setw(3) << ((stage >= 0 && stage < 4) ? stages[stage] : ".");
        }
        cout << "\n";
    }
    cout << "=> 총 " << total << " 사이클\n";
}

void pipeline_demo() {
    cs::section("1. 명령어 파이프라인");
    std::vector<PipeInstr> independent = {
        {"R1 <- 1", 1, {}}, {"R2 <- 2", 2, {}}, {"R3 <- 3", 3, {}}, {"R4 <- 4", 4, {}}, {"R5 <- 5", 5, {}}};
    cout << "의존성 없는 명령어 5개. 파이프라인이 없으면 5 x 4단계 = " << independent.size() * 4 << " 사이클\n";
    cout << "파이프라인이 있으면:\n";
    print_pipeline(independent, schedule_in_order(independent));

    cs::section("1-2. 데이터 위험 (블로그 예시를 확장)");
    std::vector<PipeInstr> prog = {
        {"R1 <- M(100)", 1, {}}, {"R2 <- M(101)", 2, {}}, {"R3 <- R1 + R2", 3, {1, 2}},
        {"R4 <- R3 + 1", 4, {3}}, {"R5 <- 1", 5, {}},    {"R6 <- 2", 6, {}},
        {"R7 <- 3", 7, {}},
    };
    cout << "R3은 R1, R2를, R4는 R3을 기다려야 한다 (순차 처리, '.'은 멈춘 사이클):\n";
    print_pipeline(prog, schedule_in_order(prog));

    cs::section("2. 비순차 명령어 처리 (OoOE)");
    cout << "의존성 없는 R5~R7을 먼저 실행해 빈 사이클을 메운다 (최종 결과는 같다):\n";
    print_pipeline(prog, schedule_out_of_order(prog));
}

// ---------------------------------------------------------------
// 3. 분기 예측
// ---------------------------------------------------------------
long long sum_big_values(const std::vector<int>& data) {
    long long sum = 0;
    for (int repeat = 0; repeat < 20; ++repeat) {
        for (int v : data) {
            if (v >= 128) sum += v;  // 이 분기의 결과를 CPU가 얼마나 잘 맞히는가?
        }
    }
    return sum;
}

void branch_prediction_demo() {
    cs::section("3. 분기 예측");
    std::vector<int> data(1 << 20);
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 255);
    for (int& v : data) v = dist(rng);

    cs::Stopwatch sw;
    long long r1 = sum_big_values(data);
    double random_ms = sw.ms();

    std::sort(data.begin(), data.end());
    sw.reset();
    long long r2 = sum_big_values(data);
    double sorted_ms = sw.ms();

    cout << "무작위 데이터: " << std::fixed << std::setprecision(1) << random_ms << " ms  (결과 " << r1 << ")\n";
    cout << "정렬된 데이터: " << sorted_ms << " ms  (결과 " << r2 << ")\n";
    cout << "=> 정렬되면 분기 결과가 '계속 아니오 ... 계속 예' 패턴이라 예측이 거의 다 맞는다.\n"
            "   (컴파일러가 분기를 없애는 cmov 명령어로 최적화하면 차이가 사라질 수도 있다. Debug/Release 비교해 보기)\n";
}

// ---------------------------------------------------------------
// 4. 멀티코어 / 멀티스레드
// ---------------------------------------------------------------
bool is_prime(int n) {
    if (n < 2) return false;
    for (int d = 2; d * d <= n; ++d) {
        if (n % d == 0) return false;
    }
    return true;
}

enum class Split {
    Dynamic,  // 작은 블록(1024개)을 공용 카운터에서 하나씩 가져간다 -> 먼저 끝난 스레드가 더 가져가므로 균등
    Stride,   // 스레드 k가 k, k+T, k+2T ...를 맡는다 -> T가 짝수면 짝수 번호 스레드는 짝수만 받는 함정!
};

struct ParallelResult {
    double total_ms;
    int primes;
    std::vector<double> thread_ms;  // 스레드별로 일한 시간
};

ParallelResult count_primes_parallel(int limit, int threads, Split split) {
    std::atomic<int> total{0}, next_block{0};
    std::vector<double> thread_ms(threads);
    cs::Stopwatch sw;
    std::vector<std::thread> workers;
    for (int t = 0; t < threads; ++t) {
        workers.emplace_back([=, &total, &next_block, &thread_ms] {
            cs::Stopwatch mine;
            int local = 0;
            if (split == Split::Dynamic) {
                const int block = 1024;
                for (int begin; (begin = next_block.fetch_add(block)) < limit;) {
                    for (int n = begin; n < std::min(begin + block, limit); ++n) local += is_prime(n);
                }
            } else {
                for (int n = t; n < limit; n += threads) local += is_prime(n);
            }
            total += local;
            thread_ms[t] = mine.ms();
        });
    }
    for (auto& w : workers) w.join();
    return {sw.ms(), total, thread_ms};
}

void multicore_demo() {
    cs::section("4. 멀티코어와 멀티스레드");
    unsigned logical = std::thread::hardware_concurrency();
    cout << "이 컴퓨터의 논리 프로세서(하드웨어 스레드) 수: " << logical << "\n\n";

    const int limit = 3'000'000;
    ParallelResult single = count_primes_parallel(limit, 1, Split::Dynamic);
    cout << limit << " 미만 소수 개수 = " << single.primes << "\n";
    cout << "스레드  시간(ms)  속도 향상\n";
    for (int t : {1, 2, 4, 8, 16, 32}) {
        if (t > static_cast<int>(logical) * 2) break;
        double ms = (t == 1) ? single.total_ms : count_primes_parallel(limit, t, Split::Dynamic).total_ms;
        cout << std::setw(5) << t << std::setw(10) << std::fixed << std::setprecision(1) << ms << std::setw(9)
             << std::setprecision(2) << single.total_ms / ms << "x"
             << (t > static_cast<int>(logical) ? "  <- 논리 프로세서보다 많으면 더 빨라지지 않는다" : "") << "\n";
    }

    int threads = static_cast<int>(std::min(8u, std::max(2u, logical)));
    cout << "\n" << threads << "스레드, 작업 분배 방식 비교 (스레드별로 일한 시간, ms)\n";
    for (Split split : {Split::Dynamic, Split::Stride}) {
        ParallelResult r = count_primes_parallel(limit, threads, split);
        cout << (split == Split::Dynamic ? "  블록 단위 동적 분배: " : "  간격 T로 나누기   : ") << std::setprecision(1)
             << std::setw(6) << r.total_ms << " ms  [";
        for (double ms : r.thread_ms) cout << std::setw(7) << ms;
        cout << " ]\n";
    }
    cout << "=> 간격 분배에서 짝수 번호 스레드는 짝수(바로 소수 아님 판정)만 받아 일찍 놀고,\n"
            "   홀수 번호 스레드만 일한다. 코어 수만 늘린다고 빨라지지 않는 이유: 작업을 고르게 나눠야 한다.\n";
}

int main() {
    cs::init_console();
    cs::title("컴퓨터 구조 - CPU 성능 향상 기법");
    pipeline_demo();
    branch_prediction_demo();
    multicore_demo();
    return 0;
}

// 직접 해보기
//  - schedule_in_order에서 '+2'를 '+1'로 바꾸면 무엇을 가정한 것인지 생각해 보기 (힌트: 포워딩)
//  - 슈퍼스칼라: 한 사이클에 명령어 2개씩 인출하도록 schedule_out_of_order를 고쳐 보기
//  - 논리 프로세서 수보다 스레드를 더 늘리면 속도가 어떻게 되는지 관찰하기
