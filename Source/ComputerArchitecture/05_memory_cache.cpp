// [컴퓨터 구조] 메모리와 캐시 메모리
// 블로그: https://bb0ng2.tistory.com/36
//
// 실습 내용
//   1. MMU: 논리 주소 -> 물리 주소 변환(베이스 레지스터), 한계 레지스터로 메모리 보호(트랩)
//   2. 저장 장치 계층: 접근하는 데이터 크기를 늘려 가며 L1 -> L2 -> L3 -> RAM으로 느려지는 것을 직접 측정
//   3. 공간 지역성: 2차원 배열을 행 우선 / 열 우선으로 순회했을 때의 속도 차이
//   4. 캐시 시뮬레이터: 캐시 적중률 = 적중 / (적중 + 미스) 계산, 캐시 라인(블록) 단위 전송
#include "cs_common.h"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <list>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using std::cout;

// ---------------------------------------------------------------
// 1. MMU
// ---------------------------------------------------------------
class MMU {
public:
    MMU(uint32_t base, uint32_t limit) : base_(base), limit_(limit) {}

    uint32_t translate(uint32_t logical) const {
        if (logical >= limit_) {
            throw std::out_of_range("트랩 발생: 논리 주소 " + std::to_string(logical) + "는 한계 " +
                                    std::to_string(limit_) + " 밖 (다른 프로그램 영역 침범)");
        }
        return base_ + logical;  // 물리 주소 = 베이스 레지스터 + 논리 주소
    }

private:
    uint32_t base_;   // 프로그램의 시작 물리 주소
    uint32_t limit_;  // 프로그램 크기 (논리 주소의 최대 범위)
};

void mmu_demo() {
    cs::section("1. MMU: 논리 주소 -> 물리 주소");
    MMU program_a(15000, 1000);  // 블로그 예시: 프로그램 A는 물리 주소 15000 ~ 15999
    MMU program_b(30000, 500);
    for (uint32_t logical : {0u, 100u, 999u, 1500u}) {
        cout << "  A의 논리 주소 " << std::setw(4) << logical << " -> ";
        try {
            uint32_t physical = program_a.translate(logical);
            cout << "물리 주소 " << physical << "\n";
        } catch (const std::exception& e) {
            cout << e.what() << "\n";
        }
    }
    cout << "  B의 논리 주소  100 -> 물리 주소 " << program_b.translate(100)
         << "  (A와 B 모두 0번지부터 시작한다고 생각하지만 실제 위치는 다르다)\n";

    // 실제 프로그램의 주소도 논리(가상) 주소다.
    int local = 0;
    cout << "  이 프로그램 변수 local의 주소: " << &local << "  <- 운영체제가 준 가상 주소\n";
}

// ---------------------------------------------------------------
// 2. 저장 장치 계층: 포인터 추적(pointer chasing)으로 메모리 지연 시간 측정
// ---------------------------------------------------------------
struct alignas(64) Node {  // 노드 하나 = 캐시 라인 하나(64바이트)
    Node* next;
};

double measure_latency_ns(size_t bytes) {
    size_t count = std::max<size_t>(bytes / sizeof(Node), 2);
    std::vector<Node> nodes(count);
    // 노드들을 무작위 순서의 하나의 원으로 잇는다 (Sattolo 알고리즘) -> 하드웨어 프리페치가 예측 못함
    std::vector<size_t> order(count);
    std::iota(order.begin(), order.end(), 0);
    std::mt19937_64 rng(7);
    for (size_t i = count - 1; i > 0; --i) {
        std::uniform_int_distribution<size_t> pick(0, i - 1);
        std::swap(order[i], order[pick(rng)]);
    }
    for (size_t i = 0; i < count; ++i) nodes[order[i]].next = &nodes[order[(i + 1) % count]];

    const size_t steps = 4'000'000;
    Node* p = &nodes[0];
    for (size_t i = 0; i < count; ++i) p = p->next;  // 미리 한 바퀴 돌아 캐시를 데운다
    cs::Stopwatch sw;
    for (size_t i = 0; i < steps; ++i) p = p->next;  // 다음 주소는 이전 읽기가 끝나야 알 수 있다
    double ns = sw.ms() * 1e6 / steps;
    if (p == nullptr) cout << "";  // 최적화로 반복문이 사라지지 않게 결과를 사용
    return ns;
}

void memory_hierarchy_demo() {
    cs::section("2. 저장 장치 계층 측정 (접근 1회당 평균 시간)");
    cout << "데이터 크기를 키우면 L1 -> L2 -> L3 캐시를 넘어 RAM까지 가면서 느려진다.\n\n";
    cout << "  크기        ns/접근\n";
    for (size_t kb = 4; kb <= 128 * 1024; kb *= 2) {
        double ns = measure_latency_ns(kb * 1024);
        std::string label = (kb >= 1024) ? std::to_string(kb / 1024) + " MB" : std::to_string(kb) + " KB";
        int bar = std::min(50, static_cast<int>(ns / 2) + 1);
        cout << "  " << std::left << std::setw(10) << label << std::right << std::fixed << std::setprecision(1)
             << std::setw(7) << ns << "  " << std::string(bar, '#') << "\n";
    }
    cout << "=> 계단처럼 꺾이는 지점이 각 캐시의 크기다. 작업 관리자 > 성능 > CPU에서 L1/L2/L3 크기와 비교해 보기\n";
}

// ---------------------------------------------------------------
// 3. 공간 지역성: 행 우선 vs 열 우선
// ---------------------------------------------------------------
void locality_demo() {
    cs::section("3. 공간 지역성: 2차원 배열 순회 순서");
    const int n = 4096;
    std::vector<int> matrix(static_cast<size_t>(n) * n, 1);  // 64MB. C++ 배열은 행 단위로 메모리에 연속 저장
    long long sum = 0;

    cs::Stopwatch sw;
    for (int row = 0; row < n; ++row)
        for (int col = 0; col < n; ++col) sum += matrix[static_cast<size_t>(row) * n + col];  // 옆 칸으로 이동
    double row_ms = sw.ms();

    sw.reset();
    for (int col = 0; col < n; ++col)
        for (int row = 0; row < n; ++row) sum += matrix[static_cast<size_t>(row) * n + col];  // 16KB씩 건너뜀
    double col_ms = sw.ms();

    cout << std::fixed << std::setprecision(1);
    cout << "  행 우선 순회: " << row_ms << " ms\n";
    cout << "  열 우선 순회: " << col_ms << " ms  (약 " << col_ms / row_ms << "배 느림, 합계 " << sum << ")\n";
    cout << "=> 캐시는 64바이트(int 16개)를 한 번에 가져온다. 행 우선이면 가져온 16개를 다 쓰고,\n"
            "   열 우선이면 1개만 쓰고 버린다.\n";
}

// ---------------------------------------------------------------
// 4. 캐시 시뮬레이터 (집합 연관 캐시, LRU 교체)
// ---------------------------------------------------------------
class CacheSimulator {
public:
    CacheSimulator(size_t cache_bytes, size_t line_bytes, size_t ways)
        : line_bytes_(line_bytes), ways_(ways), sets_(cache_bytes / line_bytes / ways), lru_(sets_) {}

    void access(uint64_t address) {
        uint64_t block = address / line_bytes_;  // 같은 블록(캐시 라인)이면 함께 캐시에 올라와 있다
        auto& set = lru_[block % sets_];
        auto it = std::find(set.begin(), set.end(), block);
        if (it != set.end()) {
            ++hits_;
            set.erase(it);
        } else {
            ++misses_;
            if (set.size() == ways_) set.pop_back();  // 가장 오래 안 쓴 라인을 내보낸다
        }
        set.push_front(block);
    }

    double hit_ratio() const { return 100.0 * hits_ / (hits_ + misses_); }
    std::string report() const {
        return "적중 " + std::to_string(hits_) + ", 미스 " + std::to_string(misses_);
    }

private:
    size_t line_bytes_, ways_, sets_;
    std::vector<std::list<uint64_t>> lru_;
    uint64_t hits_ = 0, misses_ = 0;
};

void cache_simulator_demo() {
    cs::section("4. 캐시 시뮬레이터 (1KB, 64B 라인, 2-way)");
    const int n = 64;  // 64x64 int 배열 = 16KB, 캐시(1KB)보다 훨씬 크다

    CacheSimulator row_major(1024, 64, 2), col_major(1024, 64, 2), temporal(1024, 64, 2);
    for (int r = 0; r < n; ++r)
        for (int c = 0; c < n; ++c) row_major.access((static_cast<uint64_t>(r) * n + c) * sizeof(int));
    for (int c = 0; c < n; ++c)
        for (int r = 0; r < n; ++r) col_major.access((static_cast<uint64_t>(r) * n + c) * sizeof(int));
    // 시간 지역성: 작은 배열(int 64개 = 256B)을 100번 반복해서 읽기
    for (int repeat = 0; repeat < 100; ++repeat)
        for (int i = 0; i < 64; ++i) temporal.access(static_cast<uint64_t>(i) * sizeof(int));

    cout << std::fixed << std::setprecision(1);
    cout << "  행 우선 순회     적중률 " << std::setw(5) << row_major.hit_ratio() << "%  (" << row_major.report() << ")\n";
    cout << "  열 우선 순회     적중률 " << std::setw(5) << col_major.hit_ratio() << "%  (" << col_major.report() << ")\n";
    cout << "  작은 배열 반복   적중률 " << std::setw(5) << temporal.hit_ratio() << "%  (" << temporal.report() << ")\n";
    cout << "=> 행 우선: 라인 하나(int 16개)당 미스 1번 -> 15/16 = 93.8% (공간 지역성)\n"
            "   반복 접근: 처음 4번만 미스 (시간 지역성)\n";
}

int main() {
    cs::init_console();
    cs::title("컴퓨터 구조 - 메모리와 캐시 메모리");
    mmu_demo();
    memory_hierarchy_demo();
    locality_demo();
    cache_simulator_demo();
    return 0;
}

// 직접 해보기
//  - CacheSimulator의 ways를 1(직접 사상), 16(완전 연관에 가까움)으로 바꿔 열 우선 적중률 비교
//  - 캐시 라인 크기를 16B / 128B로 바꾸면 행 우선 적중률이 어떻게 바뀌는지 예측하고 확인
