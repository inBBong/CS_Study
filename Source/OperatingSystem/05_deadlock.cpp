// [운영체제] 교착 상태
// 블로그: https://bb0ng2.tistory.com/50
//
// 실습 내용
//   1. 식사하는 철학자: 모두 왼쪽 포크부터 들면 교착 상태가 실제로 일어나는 것을 보고, 시간 초과로 검출 후 회복
//   2. 자원 할당 그래프에서 사이클 찾기
//   3. 예방 (1) 원형 대기 없애기: 포크에 번호를 붙이고 작은 번호부터 집기
//   4. 예방 (2) 점유와 대기 없애기: 두 포크를 한 번에 집거나 하나도 안 집기 (std::scoped_lock)
//   5. 회피: 은행원 알고리즘으로 안전 상태 / 불안전 상태 판단 (블로그 예제)
#include "cs_common.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using std::cout;
using namespace std::chrono_literals;

constexpr int kPhilosophers = 5;
std::mutex g_print;

template <typename... Args>
void say(Args&&... args) {
    std::lock_guard<std::mutex> lock(g_print);
    ((cout << std::forward<Args>(args)), ...);
    cout << "\n";
}

// ---------------------------------------------------------------
// 1. 교착 상태 발생 -> 검출 -> 회복
// ---------------------------------------------------------------
void deadlock_demo() {
    cs::section("1. 식사하는 철학자: 모두 왼쪽 포크부터 집기");
    std::array<std::timed_mutex, kPhilosophers> forks;
    std::atomic<int> stuck{0};

    auto philosopher = [&](int i) {
        int left = i, right = (i + 1) % kPhilosophers;
        forks[left].lock();
        say("  P", i, ": 왼쪽 포크 ", left, " 집음");
        std::this_thread::sleep_for(100ms);  // 모두가 왼쪽 포크를 집을 시간을 준다
        if (forks[right].try_lock_for(1s)) {
            say("  P", i, ": 식사");
            forks[right].unlock();
        } else {
            ++stuck;
            say("  P", i, ": 오른쪽 포크 ", right, "를 1초 동안 기다렸지만 실패");
            std::this_thread::sleep_for(100ms);  // 모두 실패를 확인할 때까지 포크를 들고 있는다
        }
        forks[left].unlock();  // 회복: 들고 있던 자원을 내려놓는다
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < kPhilosophers; ++i) threads.emplace_back(philosopher, i);
    for (auto& t : threads) t.join();
    if (stuck == kPhilosophers)
        cout << "  => 5명 모두 왼쪽 포크를 든 채 오른쪽을 기다렸다: 교착 상태 검출. 포크를 내려놓아 회복했다.\n"
                "     try_lock_for 대신 lock()이었다면 프로그램이 영원히 멈췄을 것이다.\n";
    else
        cout << "  => 이번에는 운 좋게 교착 상태를 피했다 (" << stuck << "명 실패). 다시 실행해 보자.\n";
}

// ---------------------------------------------------------------
// 2. 자원 할당 그래프에서 사이클 찾기
// ---------------------------------------------------------------
void resource_allocation_graph_demo() {
    cs::section("2. 자원 할당 그래프");
    // 노드 0~4: 철학자 P0~P4, 노드 5~9: 포크 F0~F4
    const int n = kPhilosophers * 2;
    std::vector<std::vector<int>> edges(n);
    auto name = [](int v) { return (v < kPhilosophers ? "P" : "F") + std::to_string(v % kPhilosophers); };
    for (int i = 0; i < kPhilosophers; ++i) {
        int fork_left = kPhilosophers + i;
        int fork_right = kPhilosophers + (i + 1) % kPhilosophers;
        edges[fork_left].push_back(i);   // 할당 간선: 포크 -> 철학자 (들고 있음)
        edges[i].push_back(fork_right);  // 요청 간선: 철학자 -> 포크 (기다림)
    }
    for (int v = 0; v < n; ++v)
        for (int w : edges[v])
            cout << "  " << name(v) << " -> " << name(w) << (v < kPhilosophers ? "  (요청)" : "  (할당)") << "\n";

    // DFS로 사이클 찾기
    std::vector<int> color(n, 0), parent(n, -1);  // 0: 미방문, 1: 탐색 중, 2: 완료
    std::vector<int> cycle;
    std::function<bool(int)> dfs = [&](int v) {
        color[v] = 1;
        for (int w : edges[v]) {
            if (color[w] == 1) {  // 탐색 중인 노드를 다시 만남 = 사이클
                for (int x = v; x != w; x = parent[x]) cycle.push_back(x);
                cycle.push_back(w);
                std::reverse(cycle.begin(), cycle.end());
                return true;
            }
            if (color[w] == 0) {
                parent[w] = v;
                if (dfs(w)) return true;
            }
        }
        color[v] = 2;
        return false;
    };
    if (dfs(0)) {
        cout << "  사이클 발견: ";
        for (int v : cycle) cout << name(v) << " -> ";
        cout << name(cycle.front()) << "\n";
        cout << "  => 자원마다 인스턴스가 하나뿐이므로 사이클 = 교착 상태 (원형 대기)\n";
    }
}

// ---------------------------------------------------------------
// 3, 4. 교착 상태 예방
// ---------------------------------------------------------------
void dine(const std::string& title, std::function<void(std::array<std::mutex, kPhilosophers>&, int)> eat) {
    cs::section(title);
    std::array<std::mutex, kPhilosophers> forks;
    std::array<int, kPhilosophers> meals{};
    std::vector<std::thread> threads;
    cs::Stopwatch sw;
    for (int i = 0; i < kPhilosophers; ++i) {
        threads.emplace_back([&, i] {
            for (int round = 0; round < 1000; ++round) {
                eat(forks, i);
                ++meals[i];
            }
        });
    }
    for (auto& t : threads) t.join();
    cout << "  식사 횟수:";
    for (int i = 0; i < kPhilosophers; ++i) cout << " P" << i << "=" << meals[i];
    cout << "  (" << static_cast<int>(sw.ms()) << " ms, 교착 상태 없음)\n";
}

void prevention_demo() {
    dine("3. 예방: 원형 대기 없애기 (번호가 작은 포크부터)", [](auto& forks, int i) {
        int a = i, b = (i + 1) % kPhilosophers;
        int first = std::min(a, b), second = std::max(a, b);  // P4는 포크 0을 먼저 집는다 -> 사이클이 끊어진다
        std::lock_guard<std::mutex> l1(forks[first]);
        std::lock_guard<std::mutex> l2(forks[second]);
    });
    dine("4. 예방: 점유와 대기 없애기 (두 포크를 한 번에)", [](auto& forks, int i) {
        // scoped_lock은 하나라도 못 잡으면 잡은 것을 내려놓고 다시 시도한다
        std::scoped_lock both(forks[i], forks[(i + 1) % kPhilosophers]);
    });
}

// ---------------------------------------------------------------
// 5. 은행원 알고리즘
// ---------------------------------------------------------------
struct BankerState {
    std::vector<std::string> names;
    std::vector<std::vector<int>> max_need;   // 최대 요구량
    std::vector<std::vector<int>> allocated;  // 현재 할당량
    std::vector<int> available;               // 남은 자원
};

std::optional<std::vector<int>> find_safe_sequence(const BankerState& s) {
    size_t n = s.names.size(), m = s.available.size();
    std::vector<int> work = s.available, sequence;
    std::vector<bool> finished(n, false);
    while (sequence.size() < n) {
        bool progressed = false;
        for (size_t i = 0; i < n; ++i) {
            if (finished[i]) continue;
            bool can_finish = true;
            for (size_t r = 0; r < m; ++r)
                if (s.max_need[i][r] - s.allocated[i][r] > work[r]) can_finish = false;
            if (can_finish) {  // 이 프로세스는 끝까지 실행할 수 있다 -> 끝나면 자원을 돌려준다
                for (size_t r = 0; r < m; ++r) work[r] += s.allocated[i][r];
                finished[i] = true;
                sequence.push_back(static_cast<int>(i));
                progressed = true;
            }
        }
        if (!progressed) return std::nullopt;  // 아무도 끝낼 수 없다 = 불안전 상태
    }
    return sequence;
}

void print_banker(const BankerState& s) {
    cout << "  프로세스  최대  보유  필요\n";
    for (size_t i = 0; i < s.names.size(); ++i)
        cout << "  " << s.names[i] << "        " << s.max_need[i][0] << (s.max_need[i][0] < 10 ? "     " : "    ")
             << s.allocated[i][0] << "     " << s.max_need[i][0] - s.allocated[i][0] << "\n";
    cout << "  남은 자원: " << s.available[0] << "\n";
    if (auto seq = find_safe_sequence(s)) {
        cout << "  => 안전 상태. 안전 순서열: ";
        for (size_t k = 0; k < seq->size(); ++k) cout << (k ? " -> " : "") << s.names[(*seq)[k]];
        cout << "\n";
    } else {
        cout << "  => 불안전 상태. 안전 순서열이 없다 (교착 상태가 일어날 수 있음)\n";
    }
}

// 요청을 들어줬다고 가정하고 안전한지 확인한 뒤에만 실제로 할당한다 (회피)
bool request(BankerState& s, int process, const std::vector<int>& amount) {
    BankerState trial = s;
    for (size_t r = 0; r < amount.size(); ++r) {
        if (amount[r] > trial.available[r]) return false;
        trial.available[r] -= amount[r];
        trial.allocated[process][r] += amount[r];
    }
    if (!find_safe_sequence(trial)) return false;
    s = trial;
    return true;
}

void banker_demo() {
    cs::section("5. 회피: 은행원 알고리즘 (블로그 예제, 전체 자원 12개)");
    BankerState s{{"P1", "P2", "P3"}, {{10}, {4}, {9}}, {{5}, {2}, {2}}, {3}};
    print_banker(s);

    cout << "\n  P3가 자원 1개를 더 요청하면?\n";
    BankerState unsafe = s;
    unsafe.allocated[2][0] += 1;
    unsafe.available[0] -= 1;
    print_banker(unsafe);
    cout << "  은행원 알고리즘의 판단: P3 요청 " << (request(s, 2, {1}) ? "승인" : "거절 (불안전 상태가 되므로 기다리게 함)")
         << "\n";
    cout << "  P2가 자원 2개를 요청하면? " << (request(s, 1, {2}) ? "승인 (여전히 안전)" : "거절") << "\n";
}

int main() {
    cs::init_console();
    cs::title("운영체제 - 교착 상태");
    deadlock_demo();
    resource_allocation_graph_demo();
    prevention_demo();
    banker_demo();
    return 0;
}

// 직접 해보기
//  - deadlock_demo의 sleep_for(100ms)를 지우면 교착 상태가 얼마나 자주 일어나는지 여러 번 실행해 보기
//  - 은행원 알고리즘을 자원 3종류(A, B, C)로 확장한 예제를 만들어 안전 순서열 찾기
//  - 철학자 중 1명만 오른쪽 포크부터 집게 바꿔도 교착 상태가 사라지는지 확인하기
