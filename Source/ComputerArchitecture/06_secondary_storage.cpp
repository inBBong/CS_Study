// [컴퓨터 구조] 보조기억장치
// 블로그: https://bb0ng2.tistory.com/42
//
// 실습 내용
//   1. 하드 디스크: 탐색 시간 + 회전 지연 + 전송 시간, 순차 읽기 vs 무작위 읽기
//   2. 실린더: 논리 블록 번호(LBA) -> 실린더/헤드/섹터(CHS) 변환
//   3. 플래시 메모리: 페이지 상태(FREE/VALID/INVALID), 덮어쓰기 불가, 가비지 컬렉션
//   4. RAID 0 / 1 / 5: 스트라이핑, 미러링, XOR 패리티로 고장난 디스크 복구하기
#include "cs_common.h"

#include <array>
#include <iomanip>
#include <map>
#include <optional>
#include <string>
#include <vector>

using std::cout;

// ---------------------------------------------------------------
// 1. 하드 디스크 접근 시간
// ---------------------------------------------------------------
struct HardDisk {
    int rpm;
    double avg_seek_ms;
    double transfer_mb_per_s;

    double rotational_latency_ms() const { return 0.5 * 60'000.0 / rpm; }  // 평균 반 바퀴
    double transfer_ms(double kb) const { return kb / 1024.0 / transfer_mb_per_s * 1000.0; }
};

void hdd_demo() {
    cs::section("1. 하드 디스크 접근 시간");
    HardDisk hdd{7200, 9.0, 150.0};
    cout << std::fixed << std::setprecision(3);
    cout << "7200 RPM: 1바퀴 = " << 60'000.0 / hdd.rpm << " ms, 평균 회전 지연(반 바퀴) = " << hdd.rotational_latency_ms()
         << " ms\n";
    cout << "4KB 한 블록 읽기 = 탐색 " << hdd.avg_seek_ms << " + 회전 " << hdd.rotational_latency_ms() << " + 전송 "
         << hdd.transfer_ms(4) << " = " << hdd.avg_seek_ms + hdd.rotational_latency_ms() + hdd.transfer_ms(4) << " ms\n\n";

    const int blocks = 1000;
    double random_ms = blocks * (hdd.avg_seek_ms + hdd.rotational_latency_ms() + hdd.transfer_ms(4));
    double sequential_ms = hdd.avg_seek_ms + hdd.rotational_latency_ms() + hdd.transfer_ms(4.0 * blocks);
    double ssd_random_ms = blocks * 0.08;  // NAND SSD 무작위 읽기 지연 약 80us 가정
    cout << std::setprecision(1);
    cout << "4KB x " << blocks << "개 읽기\n";
    cout << "  HDD 무작위 읽기: " << std::setw(8) << random_ms << " ms  (매번 헤드 이동 + 회전 대기)\n";
    cout << "  HDD 순차 읽기  : " << std::setw(8) << sequential_ms << " ms  (한 번만 이동하고 쭉 읽음)\n";
    cout << "  SSD 무작위 읽기: " << std::setw(8) << ssd_random_ms << " ms  (기계적 움직임이 없음)\n";
}

// ---------------------------------------------------------------
// 2. LBA -> CHS
// ---------------------------------------------------------------
void cylinder_demo() {
    cs::section("2. 실린더: 연속된 블록은 같은 실린더에 먼저 채운다");
    const int heads = 4;              // 플래터 2장 x 양면
    const int sectors_per_track = 3;  // 보기 쉽게 작게
    cout << "헤드 " << heads << "개, 트랙당 섹터 " << sectors_per_track << "개\n";
    cout << "  LBA -> (실린더, 헤드, 섹터)\n";
    for (int lba = 0; lba < 15; ++lba) {
        int cylinder = lba / (heads * sectors_per_track);
        int head = (lba / sectors_per_track) % heads;
        int sector = lba % sectors_per_track + 1;
        cout << "  " << std::setw(3) << lba << " -> (" << cylinder << ", " << head << ", " << sector << ")"
             << (lba % (heads * sectors_per_track) == 0 && lba > 0 ? "   <- 여기서 처음으로 디스크 암이 움직인다" : "")
             << "\n";
    }
}

// ---------------------------------------------------------------
// 3. 플래시 메모리와 가비지 컬렉션
// ---------------------------------------------------------------
class FlashMemory {
public:
    static constexpr int kBlocks = 4;
    static constexpr int kPagesPerBlock = 4;

    void write(const std::string& name, const std::string& data) {
        if (free_pages() <= kPagesPerBlock) garbage_collect();  // 여유 블록 1개는 GC용으로 남겨 둔다
        if (auto old = location_.find(name); old != location_.end()) {
            // 덮어쓰기는 불가능하다. 기존 페이지는 INVALID로 표시만 한다.
            page(old->second).state = State::Invalid;
        }
        int target = find_free_page(-1).value();
        pages_[target] = {State::Valid, name, data};
        location_[name] = target;
        cout << "  쓰기 " << name << "=" << data << " -> 블록 " << target / kPagesPerBlock << ", 페이지 "
             << target % kPagesPerBlock << "\n";
    }

    void print() const {
        for (int b = 0; b < kBlocks; ++b) {
            cout << "    블록 " << b << ": ";
            for (int p = 0; p < kPagesPerBlock; ++p) {
                const Page& pg = pages_[b * kPagesPerBlock + p];
                std::string cell = pg.state == State::Free ? "FREE" : (pg.state == State::Valid ? "" : "x") + pg.name + "=" + pg.data;
                cout << "[" << std::left << std::setw(7) << cell << "]" << std::right;
            }
            cout << "\n";
        }
        cout << "    (x 표시는 INVALID 페이지)\n";
    }

private:
    enum class State { Free, Valid, Invalid };
    struct Page {
        State state = State::Free;
        std::string name, data;
    };
    std::array<Page, kBlocks * kPagesPerBlock> pages_{};
    std::map<std::string, int> location_;

    Page& page(int index) { return pages_[index]; }
    int free_pages() const {
        int n = 0;
        for (const auto& p : pages_) n += (p.state == State::Free);
        return n;
    }
    std::optional<int> find_free_page(int exclude_block) const {
        for (int i = 0; i < static_cast<int>(pages_.size()); ++i)
            if (pages_[i].state == State::Free && i / kPagesPerBlock != exclude_block) return i;
        return std::nullopt;
    }

    void garbage_collect() {
        // 1) INVALID 페이지가 가장 많은 블록을 고른다
        int victim = -1, most_invalid = 0;
        for (int b = 0; b < kBlocks; ++b) {
            int invalid = 0;
            for (int p = 0; p < kPagesPerBlock; ++p) invalid += (pages_[b * kPagesPerBlock + p].state == State::Invalid);
            if (invalid > most_invalid) most_invalid = invalid, victim = b;
        }
        if (victim < 0) return;
        cout << "  ** 가비지 컬렉션: 블록 " << victim << " 선택 (INVALID " << most_invalid << "개)\n";
        // 2) VALID 페이지를 다른 블록으로 옮긴다
        for (int p = 0; p < kPagesPerBlock; ++p) {
            Page& pg = pages_[victim * kPagesPerBlock + p];
            if (pg.state != State::Valid) continue;
            int target = find_free_page(victim).value();
            pages_[target] = pg;
            location_[pg.name] = target;
            cout << "     VALID " << pg.name << " 복사 -> 블록 " << target / kPagesPerBlock << "\n";
        }
        // 3) 블록 단위로 삭제한다 (플래시는 페이지 단위로 지울 수 없다)
        for (int p = 0; p < kPagesPerBlock; ++p) pages_[victim * kPagesPerBlock + p] = Page{};
        cout << "     블록 " << victim << " 삭제 -> 모든 페이지 FREE\n";
    }
};

void flash_demo() {
    cs::section("3. 플래시 메모리: 덮어쓰기 대신 INVALID 표시, 블록 단위 삭제");
    FlashMemory ssd;
    for (std::string name : {"A", "B", "C", "D", "E", "F"}) ssd.write(name, "1");
    ssd.print();
    cout << "\n  A, B, C를 수정 (새 페이지에 쓰고 옛 페이지는 INVALID)\n";
    for (std::string name : {"A", "B", "C"}) ssd.write(name, "2");
    for (std::string name : {"G", "H", "I"}) ssd.write(name, "1");
    ssd.print();
    cout << "\n  여유 공간이 부족한 상태에서 J를 쓰면?\n";
    ssd.write("J", "1");
    ssd.print();
}

// ---------------------------------------------------------------
// 4. RAID
// ---------------------------------------------------------------
using Disk = std::vector<char>;

void print_disks(const std::vector<Disk>& disks, const std::vector<std::vector<bool>>* is_parity = nullptr) {
    for (size_t d = 0; d < disks.size(); ++d) {
        cout << "    디스크 " << d << ": ";
        for (size_t i = 0; i < disks[d].size(); ++i) {
            bool parity = is_parity && (*is_parity)[d][i];
            if (parity)
                cout << "P(" << std::hex << std::setw(2) << std::setfill('0') << (static_cast<unsigned>(disks[d][i]) & 0xFF)
                     << std::dec << std::setfill(' ') << ") ";
            else
                cout << "  " << (disks[d][i] ? disks[d][i] : '?') << "   ";
        }
        cout << "\n";
    }
}

void raid_demo() {
    cs::section("4. RAID");
    const std::string data = "ABCDEFGHI";

    cout << "RAID 0 (스트라이핑, 디스크 3개): 나눠서 저장 -> 3개를 동시에 읽어 빠름, 하나만 고장나도 전부 잃음\n";
    std::vector<Disk> raid0(3);
    for (size_t i = 0; i < data.size(); ++i) raid0[i % 3].push_back(data[i]);
    print_disks(raid0);

    cout << "\nRAID 1 (미러링, 디스크 2개): 똑같이 복사 -> 안전하지만 용량 50%만 사용\n";
    std::vector<Disk> raid1(2, Disk(data.begin(), data.end()));
    print_disks(raid1);

    cout << "\nRAID 5 (분산 패리티, 디스크 4개): 줄(stripe)마다 데이터 3개 + 패리티 1개, 패리티 위치를 돌아가며 둔다\n";
    const int n = 4;
    std::vector<Disk> raid5(n);
    std::vector<std::vector<bool>> parity_mark(n);
    for (size_t stripe = 0; stripe * (n - 1) < data.size(); ++stripe) {
        int parity_disk = static_cast<int>(n - 1 - stripe % n);
        char parity = 0;
        size_t next = stripe * (n - 1);
        for (int d = 0; d < n; ++d) {
            if (d == parity_disk) continue;
            char c = next < data.size() ? data[next++] : 0;
            raid5[d].push_back(c);
            parity_mark[d].push_back(false);
            parity ^= c;  // 패리티 = 같은 줄의 데이터를 모두 XOR
        }
        raid5[parity_disk].push_back(parity);
        parity_mark[parity_disk].push_back(true);
    }
    print_disks(raid5, &parity_mark);

    const int failed = 1;
    cout << "\n  디스크 " << failed << " 고장!\n";
    Disk lost = raid5[failed];
    std::fill(raid5[failed].begin(), raid5[failed].end(), char{0});
    print_disks(raid5, &parity_mark);

    // 복구: 나머지 디스크를 XOR하면 사라진 값이 나온다 (A ^ B ^ C ^ P = 0 이므로)
    for (size_t i = 0; i < raid5[failed].size(); ++i) {
        char recovered = 0;
        for (int d = 0; d < n; ++d)
            if (d != failed) recovered ^= raid5[d][i];
        raid5[failed][i] = recovered;
    }
    cout << "\n  나머지 디스크를 XOR해서 복구:\n";
    print_disks(raid5, &parity_mark);
    cout << "  복구 결과가 원본과 " << (raid5[failed] == lost ? "같다" : "다르다") << "\n";

    cout << "\n용량 효율 (디스크 " << n << "개): RAID0 100%, RAID1 50%, RAID5 " << 100 * (n - 1) / n << "%, RAID6 "
         << 100 * (n - 2) / n << "%\n";
}

int main() {
    cs::init_console();
    cs::title("컴퓨터 구조 - 보조기억장치");
    hdd_demo();
    cylinder_demo();
    flash_demo();
    raid_demo();
    return 0;
}

// 직접 해보기
//  - RAID 5에서 디스크 2개를 동시에 고장 내면 복구가 왜 안 되는지 확인하기 (RAID 6가 필요한 이유)
//  - FlashMemory에 같은 파일을 계속 수정하면서 블록별 삭제 횟수를 세어 보기 (수명과 웨어 레벨링)
