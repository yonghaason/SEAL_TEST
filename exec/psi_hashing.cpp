//   빌드: cmake --build build --target psi_hashing
//   실행: ./build/psi_hashing
//
// 강의 시연용 (암호 없음, 채울 곳 없음). 실험 9 에서 쓰는 해싱이 왜 그렇게 생겼는지 숫자로 본다.
//   1. 보내는 쪽 simple hashing: 해시 함수 수에 따라 bucket 크기가 어떻게 되는가
//   2. 받는 쪽도 일반 해시 테이블로 넣으면: 칸마다 개수가 달라 가장 많은 칸에 맞춰 채워야 한다 -> cuckoo hashing
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
    // 2. 받는 쪽도 일반 해시 테이블 (해시 함수 하나) 로 넣으면: 칸마다 원소 수가 다르다.
    //    슬롯 하나에는 원소 하나만 들어가므로, 가장 많이 든 칸의 원소 수만큼 암호문이 필요하고
    //    나머지 칸은 가짜 원소로 채워야 한다 (안 채우면 칸별 개수로 받는 쪽 원소의 분포가 드러난다).
    // ---------------------------------------------------------------
    {
        unordered_set<uint64_t> used;
        vector<uint64_t> Y = random_items(4000, t, rng, used);
        vector<int> count(N, 0);
        for (uint64_t y : Y)
        {
            count[hash_bucket(0, y, N)]++;
        }
        int max_load = *max_element(count.begin(), count.end());
        vector<int> hist(max_load + 1, 0);
        for (int c : count)
        {
            hist[c]++;
        }
        cout << "\n2. receiver: |Y| = " << Y.size() << " into N = " << N << " slots with one hash function\n";
        cout << setw(16) << "items in slot" << setw(10) << "slots" << "\n";
        for (int c = 0; c <= max_load; c++)
        {
            cout << setw(16) << c << setw(10) << hist[c] << "\n";
        }
        cout << "-> " << max_load << " ciphertexts needed (padding " << static_cast<long>(max_load) * N - Y.size()
             << " dummy items),  cuckoo hashing: 1 ciphertext\n";
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
