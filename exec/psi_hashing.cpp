//   빌드: cmake --build build --target psi_hashing
//   실행: ./build/psi_hashing
//
// 강의 시연용 (암호 없음, 채울 곳 없음). 실험 9 에서 쓰는 해싱이 왜 그렇게 생겼는지 숫자로 본다.
//   1. 보내는 쪽 simple hashing: 해시 함수 수에 따라 bucket 크기가 어떻게 되는가
//   2. 받는 쪽: 해시 함수 하나로 넣으면 칸이 겹친다 -> cuckoo hashing
//   3. cuckoo hashing: 해시 함수 수와 테이블을 얼마나 채우는지에 따른 실패율
#include <random>
#include "psi_common.h"

using namespace std;

int main()
{
    const uint64_t t = 1032193;  // 실험 7 ~ 9 의 plain modulus (20 비트)
    const size_t N = 8192;       // 해시 테이블 크기 = 슬롯 수
    mt19937_64 rng(1);

    // ---------------------------------------------------------------
    // 1. 보내는 쪽 simple hashing: |X| = 2^18 을 N = 8192 칸에
    //    원소 하나가 h 칸에 들어가므로 평균은 h |X| / N. 다항식 차수는 가장 큰 bucket 이 정한다.
    // ---------------------------------------------------------------
    {
        unordered_set<uint64_t> used;
        vector<uint64_t> X = random_items(size_t(1) << 18, t, rng, used);
        cout << "1. sender: simple hashing of |X| = " << X.size() << " into N = " << N << " buckets\n";
        cout << setw(10) << "hashes" << setw(10) << "average" << setw(10) << "max" << "\n";
        for (int h : { 1, 2, 3 })
        {
            auto bucket = simple_hash(X, N, h);
            double avg = 0;
            for (auto &b : bucket)
            {
                avg += b.size();
            }
            avg /= N;
            cout << setw(10) << h << fixed << setprecision(1) << setw(10) << avg << setw(10)
                 << max_bucket_size(bucket) << "\n";
            cout.unsetf(ios::fixed);
        }
    }
    wait_enter("다음: 받는 쪽은 왜 cuckoo hashing 인가");

    // ---------------------------------------------------------------
    // 2. 받는 쪽: |Y| = 4000 을 해시 함수 하나로 넣으면 같은 칸에 두 개 이상 들어가는 원소가 생긴다.
    //    슬롯 하나에는 원소 하나만 들어가므로, 이런 원소는 빠진다.
    // ---------------------------------------------------------------
    {
        unordered_set<uint64_t> used;
        vector<uint64_t> Y = random_items(4000, t, rng, used);
        vector<int> count(N, 0);
        for (uint64_t y : Y)
        {
            count[hash_bucket(0, y, N)]++;
        }
        int lost = 0;
        for (int c : count)
        {
            lost += max(0, c - 1);
        }
        cout << "\n2. receiver: |Y| = " << Y.size() << " with one hash function -> " << lost
             << " items collide and do not fit\n";
    }
    wait_enter("다음: cuckoo hashing 실패율");

    // ---------------------------------------------------------------
    // 3. cuckoo hashing: 해시 함수 h 개, 테이블을 |Y| / N 만큼 채울 때 실패율 (100 번 시도)
    // ---------------------------------------------------------------
    {
        const int TRIALS = 100;
        vector<double> loads = { 0.3, 0.45, 0.6, 0.75, 0.9 };
        cout << "\n3. cuckoo hashing into N = " << N << " slots: failures out of " << TRIALS << " trials\n";
        cout << setw(10) << "load" << setw(8) << "|Y|";
        for (int h : { 2, 3 })
        {
            cout << setw(10) << (to_string(h) + " hashes");
        }
        cout << "\n";
        for (double load : loads)
        {
            size_t m = static_cast<size_t>(load * N);
            cout << fixed << setprecision(2) << setw(10) << load << setw(8) << m;
            cout.unsetf(ios::fixed);
            for (int h : { 2, 3 })
            {
                int fail = 0;
                for (int trial = 0; trial < TRIALS; trial++)
                {
                    unordered_set<uint64_t> used;
                    vector<uint64_t> Y = random_items(m, t, rng, used);
                    vector<uint64_t> table;
                    if (!cuckoo_insert(Y, N, h, rng, table))
                    {
                        fail++;
                    }
                }
                cout << setw(10) << fail;
            }
            cout << "\n";
        }
    }

    return 0;
}
