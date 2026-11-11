// [운영체제] CPU 스케줄링
// 블로그: https://bb0ng2.tistory.com/48
//
// 같은 프로세스 묶음을 여러 스케줄링 알고리즘으로 돌려 보고 간트 차트와 성능 지표를 비교한다.
//
// 실습 내용
//   1. FCFS(선입 선처리)와 호위 효과
//   2. SJF(최단 작업 우선), SRT(최소 잔여 시간 우선)
//   3. 라운드 로빈과 타임 슬라이스 크기의 영향
//   4. 우선순위 스케줄링의 기아 현상과 에이징
//   5. 다단계 피드백 큐
//
// 지표
//   대기 시간    = 준비 큐에서 기다린 시간 = 완료 - 도착 - 실행 시간
//   반환 시간    = 완료 - 도착
//   응답 시간    = 처음 CPU를 받은 시각 - 도착
//   문맥 교환    = 실행 중인 프로세스가 바뀐 횟수
#include "cs_common.h"

#include <algorithm>
#include <deque>
#include <functional>
#include <iomanip>
#include <string>
#include <vector>

using std::cout;

struct Process {
    std::string name;
    int arrival;
    int burst;
    int priority = 0;  // 작을수록 높은 우선순위
};

// 타임라인: timeline[t] = t ~ t+1 동안 실행된 프로세스 번호 (-1이면 CPU가 쉼)
using Timeline = std::vector<int>;

bool all_done(const std::vector<int>& remaining) {
    return std::all_of(remaining.begin(), remaining.end(), [](int r) { return r == 0; });
}

// ---------------------------------------------------------------
// 알고리즘
// ---------------------------------------------------------------
Timeline fcfs(const std::vector<Process>& ps) {
    std::vector<int> order(ps.size());
    for (size_t i = 0; i < ps.size(); ++i) order[i] = static_cast<int>(i);
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return ps[a].arrival < ps[b].arrival; });
    Timeline tl;
    for (int i : order) {
        while (static_cast<int>(tl.size()) < ps[i].arrival) tl.push_back(-1);
        tl.insert(tl.end(), ps[i].burst, i);  // 끝날 때까지 CPU를 놓지 않는다 (비선점)
    }
    return tl;
}

// 비선점: 실행 중인 프로세스가 끝날 때만 다음을 고른다
Timeline non_preemptive(const std::vector<Process>& ps, std::function<bool(int, int)> better) {
    std::vector<int> remaining;
    for (const auto& p : ps) remaining.push_back(p.burst);
    Timeline tl;
    while (!all_done(remaining)) {
        int t = static_cast<int>(tl.size());
        int pick = -1;
        for (size_t i = 0; i < ps.size(); ++i) {
            if (remaining[i] == 0 || ps[i].arrival > t) continue;
            if (pick == -1 || better(static_cast<int>(i), pick)) pick = static_cast<int>(i);
        }
        if (pick == -1) {
            tl.push_back(-1);
            continue;
        }
        tl.insert(tl.end(), remaining[pick], pick);
        remaining[pick] = 0;
    }
    return tl;
}

Timeline sjf(const std::vector<Process>& ps) {
    return non_preemptive(ps, [&](int a, int b) { return ps[a].burst < ps[b].burst; });
}

// 선점: 매 시간 단위마다 다시 고른다
Timeline srt(const std::vector<Process>& ps) {
    std::vector<int> remaining;
    for (const auto& p : ps) remaining.push_back(p.burst);
    Timeline tl;
    while (!all_done(remaining)) {
        int t = static_cast<int>(tl.size());
        int pick = -1;
        for (size_t i = 0; i < ps.size(); ++i) {
            if (remaining[i] == 0 || ps[i].arrival > t) continue;
            if (pick == -1 || remaining[i] < remaining[pick]) pick = static_cast<int>(i);
        }
        tl.push_back(pick);
        if (pick != -1) --remaining[pick];
    }
    return tl;
}

Timeline round_robin(const std::vector<Process>& ps, int time_slice) {
    std::vector<int> remaining;
    for (const auto& p : ps) remaining.push_back(p.burst);
    std::vector<bool> arrived(ps.size(), false);
    std::deque<int> ready_queue;
    Timeline tl;
    auto admit = [&](int t) {  // 도착한 프로세스를 준비 큐에 넣는다
        for (size_t i = 0; i < ps.size(); ++i)
            if (!arrived[i] && ps[i].arrival <= t) arrived[i] = true, ready_queue.push_back(static_cast<int>(i));
    };
    admit(0);
    while (!all_done(remaining)) {
        if (ready_queue.empty()) {
            tl.push_back(-1);
            admit(static_cast<int>(tl.size()));
            continue;
        }
        int current = ready_queue.front();
        ready_queue.pop_front();
        for (int used = 0; used < time_slice && remaining[current] > 0; ++used) {
            tl.push_back(current);
            --remaining[current];
            admit(static_cast<int>(tl.size()));
        }
        if (remaining[current] > 0) ready_queue.push_back(current);  // 타임 슬라이스를 다 쓰면 큐의 맨 뒤로
    }
    return tl;
}

// 선점형 우선순위. aging_interval > 0이면 그 시간만큼 기다릴 때마다 우선순위를 1씩 높인다
Timeline priority_scheduling(const std::vector<Process>& ps, int aging_interval) {
    std::vector<int> remaining, effective, waited(ps.size(), 0);
    for (const auto& p : ps) remaining.push_back(p.burst), effective.push_back(p.priority);
    Timeline tl;
    while (!all_done(remaining)) {
        int t = static_cast<int>(tl.size());
        int pick = -1;
        for (size_t i = 0; i < ps.size(); ++i) {
            if (remaining[i] == 0 || ps[i].arrival > t) continue;
            if (pick == -1 || effective[i] < effective[pick]) pick = static_cast<int>(i);
        }
        tl.push_back(pick);
        if (pick != -1) --remaining[pick];
        for (size_t i = 0; i < ps.size(); ++i) {
            if (static_cast<int>(i) == pick || remaining[i] == 0 || ps[i].arrival > t) continue;
            if (aging_interval > 0 && ++waited[i] % aging_interval == 0) --effective[i];  // 에이징
        }
    }
    return tl;
}

// 다단계 피드백 큐: 큐 0(슬라이스 2) -> 큐 1(슬라이스 4) -> 큐 2(FCFS)
Timeline mlfq(const std::vector<Process>& ps) {
    const int slices[] = {2, 4, 1 << 30};
    std::vector<int> remaining;
    for (const auto& p : ps) remaining.push_back(p.burst);
    std::vector<bool> arrived(ps.size(), false);
    std::deque<int> queues[3];
    Timeline tl;
    auto admit = [&](int t) {
        for (size_t i = 0; i < ps.size(); ++i)
            if (!arrived[i] && ps[i].arrival <= t) arrived[i] = true, queues[0].push_back(static_cast<int>(i));
    };
    admit(0);
    while (!all_done(remaining)) {
        int level = 0;
        while (level < 3 && queues[level].empty()) ++level;
        if (level == 3) {
            tl.push_back(-1);
            admit(static_cast<int>(tl.size()));
            continue;
        }
        int current = queues[level].front();
        queues[level].pop_front();
        int used = 0;
        bool preempted = false;
        while (used < slices[level] && remaining[current] > 0) {
            tl.push_back(current);
            --remaining[current];
            ++used;
            admit(static_cast<int>(tl.size()));
            if (level > 0 && !queues[0].empty() && remaining[current] > 0) {  // 상위 큐에 새 프로세스가 오면 선점
                preempted = true;
                break;
            }
        }
        if (remaining[current] == 0) continue;
        if (!preempted && level < 2) ++level;  // 슬라이스를 다 쓰면 아래 큐로 강등 (CPU 집중 프로세스)
        queues[level].push_back(current);
    }
    return tl;
}

// ---------------------------------------------------------------
// 출력
// ---------------------------------------------------------------
void report(const std::string& title, const std::vector<Process>& ps, const Timeline& tl) {
    cout << "\n[" << title << "]\n";
    // 프로세스별 막대: '#' 실행, '.' 준비 큐에서 대기
    std::vector<int> finish(ps.size(), 0), first_run(ps.size(), -1);
    for (int t = 0; t < static_cast<int>(tl.size()); ++t) {
        if (tl[t] < 0) continue;
        finish[tl[t]] = t + 1;
        if (first_run[tl[t]] < 0) first_run[tl[t]] = t;
    }
    cout << "        ";
    for (int t = 0; t < static_cast<int>(tl.size()); ++t) cout << (t % 5 == 0 ? '|' : ' ');
    cout << "  (눈금 5단위)\n";
    for (size_t i = 0; i < ps.size(); ++i) {
        cout << "  " << std::left << std::setw(6) << ps[i].name << std::right;
        for (int t = 0; t < static_cast<int>(tl.size()); ++t) {
            if (tl[t] == static_cast<int>(i)) cout << '#';
            else if (t >= ps[i].arrival && t < finish[i]) cout << '.';
            else cout << ' ';
        }
        cout << "\n";
    }

    double total_wait = 0, total_turnaround = 0, total_response = 0;
    for (size_t i = 0; i < ps.size(); ++i) {
        int turnaround = finish[i] - ps[i].arrival;
        total_turnaround += turnaround;
        total_wait += turnaround - ps[i].burst;
        total_response += first_run[i] - ps[i].arrival;
    }
    int switches = 0;
    int last = -1;
    for (int p : tl) {
        if (p >= 0 && last >= 0 && p != last) ++switches;
        if (p >= 0) last = p;
    }
    double n = static_cast<double>(ps.size());
    cout << std::fixed << std::setprecision(2) << "  평균 대기 " << total_wait / n << "  평균 반환 " << total_turnaround / n
         << "  평균 응답 " << total_response / n << "  문맥 교환 " << switches << "회\n";
}

void print_processes(const std::vector<Process>& ps) {
    cout << "  이름   도착  실행  우선순위\n";
    for (const auto& p : ps)
        cout << "  " << std::left << std::setw(6) << p.name << std::right << std::setw(4) << p.arrival << std::setw(6)
             << p.burst << std::setw(8) << p.priority << "\n";
}

int main() {
    cs::init_console();
    cs::title("운영체제 - CPU 스케줄링");

    // 긴 작업 하나가 먼저 오고, 짧은 작업들이 뒤따라오는 상황
    std::vector<Process> ps = {{"P1", 0, 12}, {"P2", 1, 3}, {"P3", 2, 2}, {"P4", 3, 5}, {"P5", 6, 1}};
    cs::section("프로세스 목록");
    print_processes(ps);

    cs::section("1. FCFS - 호위 효과");
    report("FCFS", ps, fcfs(ps));
    cout << "  => 짧은 P2~P5가 긴 P1 뒤에서 오래 기다린다.\n";

    cs::section("2. SJF / SRT");
    report("SJF (비선점)", ps, sjf(ps));
    report("SRT (선점)", ps, srt(ps));
    cout << "  => 짧은 작업을 먼저 하면 평균 대기 시간이 줄어든다. 하지만 실행 시간을 미리 알아야 한다.\n";

    cs::section("3. 라운드 로빈 - 타임 슬라이스 크기");
    report("RR, 타임 슬라이스 1", ps, round_robin(ps, 1));
    report("RR, 타임 슬라이스 3", ps, round_robin(ps, 3));
    report("RR, 타임 슬라이스 20", ps, round_robin(ps, 20));
    cout << "  => 너무 작으면 문맥 교환이 폭증하고, 너무 크면 FCFS와 같아진다. 응답 시간도 비교해 보자.\n";

    cs::section("4. 우선순위 스케줄링 - 기아 현상과 에이징");
    // 우선순위 낮은 LOW가 먼저 왔지만 높은 우선순위 프로세스가 계속 들어온다
    std::vector<Process> starving = {{"LOW", 0, 3, 9}, {"H1", 0, 4, 1}, {"H2", 3, 4, 1},
                                     {"H3", 7, 4, 1},  {"H4", 11, 4, 1}, {"H5", 15, 4, 1}};
    print_processes(starving);
    report("우선순위 (에이징 없음)", starving, priority_scheduling(starving, 0));
    report("우선순위 + 에이징 (1단위 기다릴 때마다 우선순위 +1)", starving, priority_scheduling(starving, 1));
    cout << "  => 에이징이 없으면 LOW는 높은 우선순위가 다 끝날 때까지 굶는다.\n";

    cs::section("5. 다단계 피드백 큐 (큐0: 슬라이스 2, 큐1: 슬라이스 4, 큐2: FCFS)");
    report("MLFQ", ps, mlfq(ps));
    cout << "  => 짧은 작업(입출력 집중형과 비슷)은 위 큐에서 빨리 끝나고, 긴 P1은 아래 큐로 내려간다.\n";
    return 0;
}

// 직접 해보기
//  - 프로세스 목록을 바꿔 SJF가 FCFS보다 나빠지는 경우가 있는지 찾아보기
//  - round_robin에 문맥 교환 비용(교환할 때마다 1단위 쉼)을 넣고 타임 슬라이스별 총 시간 비교하기
//  - mlfq에 에이징을 추가해서 아래 큐에 오래 있던 프로세스를 위로 올려 보기
