//   빌드: cmake --build build --target exp3_lazy
//   실행: ./build/exp3_lazy
#include <cmath>
#include <random>
#include "common.h"

using namespace std;
using namespace seal;

// eager: 곱셈마다 relinearize 한 뒤 더하기. sum_{i<k} A[i] * B[i] 를 돌려준다.
Ciphertext eager_sum(Evaluator &evaluator, const vector<Ciphertext> &A, const vector<Ciphertext> &B, int k,
                     const RelinKeys &relin_keys)
{
    Ciphertext sum;
    for (int i = 0; i < k; i++)
    {
        Ciphertext prod;
        evaluator.multiply(A[i], B[i], prod);
        evaluator.relinearize_inplace(prod, relin_keys);
        if (i == 0)
        {
            sum = prod;
        }
        else
        {
            evaluator.add_inplace(sum, prod);
        }
    }
    return sum;
}

// lazy: 크기 3 암호문을 모두 더한 뒤 relinearize 1 번. sum_{i<k} A[i] * B[i] 를 돌려준다.
Ciphertext lazy_sum(Evaluator &evaluator, const vector<Ciphertext> &A, const vector<Ciphertext> &B, int k,
                    const RelinKeys &relin_keys)
{
    Ciphertext sum;
    for (int i = 0; i < k; i++)
    {
        Ciphertext prod;
        evaluator.multiply(A[i], B[i], prod);
        if (i == 0)
        {
            sum = prod;
        }
        else
        {
            evaluator.add_inplace(sum, prod);
        }
    }
    evaluator.relinearize_inplace(sum, relin_keys);
    return sum;
}

// special_prime_bits: special prime P 의 비트 수
void run(int special_prime_bits)
{
    // n = 16384, q = 60비트 RNS 소수 6개, t = 20비트
    EncryptionParameters parms(scheme_type::bgv);
    size_t n = 16384;
    parms.set_poly_modulus_degree(n);
    parms.set_coeff_modulus(CoeffModulus::Create(n, { 60, 60, 60, 60, 60, 60, special_prime_bits }));
    parms.set_plain_modulus(PlainModulus::Batching(n, 20));

    SEALContext context(parms);
    cout << "\n";
    print_params(context);
    uint64_t t = parms.plain_modulus().value();

    KeyGenerator keygen(context);
    PublicKey public_key;
    keygen.create_public_key(public_key);
    RelinKeys relin_keys;
    keygen.create_relin_keys(relin_keys);
    Encryptor encryptor(context, public_key);
    Decryptor decryptor(context, keygen.secret_key());
    Evaluator evaluator(context);
    BatchEncoder encoder(context);

    // 랜덤 벡터 32 쌍을 암호화. expected[k] = 앞의 k 쌍의 내적 (슬롯별)
    const int K_MAX = 32;
    mt19937_64 rng(1);
    vector<Ciphertext> A(K_MAX), B(K_MAX);
    vector<vector<uint64_t>> expected(K_MAX + 1, vector<uint64_t>(encoder.slot_count(), 0));
    for (int i = 0; i < K_MAX; i++)
    {
        vector<uint64_t> a(encoder.slot_count()), b(encoder.slot_count());
        for (size_t j = 0; j < a.size(); j++)
        {
            a[j] = rng() % t;
            b[j] = rng() % t;
            expected[i + 1][j] = (expected[i][j] + mulmod(a[j], b[j], t)) % t;
        }
        Plaintext pt_a, pt_b;
        encoder.encode(a, pt_a);
        encoder.encode(b, pt_b);
        encryptor.encrypt(pt_a, A[i]);
        encryptor.encrypt(pt_b, B[i]);
    }

    // 곱셈 1 번 + relinearize 1 번의 예산 -> key switching 에러 K 추정
    Ciphertext one;
    evaluator.multiply(A[0], B[0], one);
    int b_size3 = print_ct("ct x ct (size 3)", one, decryptor, context);
    evaluator.relinearize_inplace(one, relin_keys);
    int b_relin1 = print_ct("relinearize", one, decryptor, context);

    // 이론값: lazy 가 eager 보다 예산이 얼마나 더 남는가
    //   M: 곱셈 1 번의 에러, K: key switching (relinearize) 1 번의 에러
    //   eager 에러 = k (M + K), lazy 에러 = k M + K  ->  예산 차이 = log2( k (M + K) / (k M + K) )
    //   M = 1 로 두면 relinearize 에서 늘어난 에러 비트가 log2(M + K) 이므로 K = 2^(relin 예산 감소량) - 1
    double M = 1.0;
    double K = pow(2.0, b_size3 - b_relin1) - 1.0;

    const int R = 5; // 시간 측정을 위해 R 번 반복
    cout << "\n" << right << setw(4) << "k" << setw(14) << "budget eager" << setw(8) << "lazy" << setw(16)
         << "lazy-eager th" << setw(6) << "meas" << setw(14) << "time eager" << setw(10) << "lazy" << "  check\n";
    for (int k : { 1, 2, 4, 8, 16, 32 })
    {
        Ciphertext eager, lazy;
        Timer tm_eager;
        for (int rep = 0; rep < R; rep++)
        {
            eager = eager_sum(evaluator, A, B, k, relin_keys);
        }
        double ms_eager = tm_eager.ms() / R;
        Timer tm_lazy;
        for (int rep = 0; rep < R; rep++)
        {
            lazy = lazy_sum(evaluator, A, B, k, relin_keys);
        }
        double ms_lazy = tm_lazy.ms() / R;

        require_ct("eager", eager);
        require_ct("lazy", lazy);
        int b_eager = decryptor.invariant_noise_budget(eager);
        int b_lazy = decryptor.invariant_noise_budget(lazy);
        double theory = log2(k * (M + K)) - log2(k * M + K);

        Plaintext pe, pl;
        vector<uint64_t> ve, vl;
        decryptor.decrypt(eager, pe);
        decryptor.decrypt(lazy, pl);
        encoder.decode(pe, ve);
        encoder.decode(pl, vl);
        bool ok = (ve == expected[k]) && (vl == expected[k]);

        cout << fixed << setprecision(1) << setw(4) << k << setw(14) << b_eager << setw(8) << b_lazy << setw(16)
             << theory << setw(6) << b_lazy - b_eager << setprecision(2) << setw(11) << ms_eager << " ms"
             << setw(7) << ms_lazy << " ms" << (ok ? "  OK" : "  FAIL") << "\n";
        cout.unsetf(ios::fixed);
    }
}

int main()
{
    run(60); // special prime 이 RNS 소수만큼 클 때 (SEAL 권장)
    wait_enter("다음: special prime 을 18비트로");
    run(18); // special prime 이 RNS 소수보다 한참 작을 때

    return 0;
}
