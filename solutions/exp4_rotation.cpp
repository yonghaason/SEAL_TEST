//   빌드: cmake --build build --target exp4_rotation
//   실행: ./build/exp4_rotation
#include <random>
#include <sstream>
#include "common.h"

using namespace std;
using namespace seal;

// 슬롯을 2 x (n/2) 행렬로 보고, 각 행의 앞 4 개와 마지막 1 개를 출력한다.
void print_rows(const string &label, const Ciphertext &ct, Decryptor &decryptor, BatchEncoder &encoder)
{
    require_ct(label, ct);
    Plaintext pt;
    decryptor.decrypt(ct, pt);
    vector<uint64_t> v;
    encoder.decode(pt, v);
    size_t row = v.size() / 2;
    cout << label << "\n";
    for (size_t r = 0; r < 2; r++)
    {
        cout << "  [ " << v[r * row] << ", " << v[r * row + 1] << ", " << v[r * row + 2] << ", "
             << v[r * row + 3] << ", ..., " << v[r * row + row - 1] << " ]\n";
    }
}

// 회전 키 크기 (MB)
double key_mb(const GaloisKeys &keys)
{
    return keys.save_size(compr_mode_type::none) / 1e6;
}

int main()
{
    EncryptionParameters parms(scheme_type::bgv);
    size_t n = 8192;
    parms.set_poly_modulus_degree(n);
    parms.set_coeff_modulus(CoeffModulus::BFVDefault(n));
    parms.set_plain_modulus(PlainModulus::Batching(n, 20));

    SEALContext context(parms);
    print_params(context);

    KeyGenerator keygen(context);
    PublicKey public_key;
    keygen.create_public_key(public_key);
    Encryptor encryptor(context, public_key);
    Decryptor decryptor(context, keygen.secret_key());
    Evaluator evaluator(context);
    BatchEncoder encoder(context);

    // 슬롯에 0, 1, 2, ..., n-1 을 넣는다. 윗줄 = 0 ~ n/2-1, 아랫줄 = n/2 ~ n-1
    size_t row = n / 2;
    vector<uint64_t> m(n);
    for (size_t i = 0; i < n; i++)
    {
        m[i] = i;
    }
    Plaintext pt;
    encoder.encode(m, pt);
    Ciphertext ct;
    encryptor.encrypt(pt, ct);

    // ---------------------------------------------------------------
    // 1. 슬롯 구조: rotate_rows, rotate_columns
    // ---------------------------------------------------------------
    GaloisKeys galois_keys; // SEAL 기본 회전 키
    keygen.create_galois_keys(galois_keys);

    print_rows("original", ct, decryptor, encoder);
    print_ct("fresh", ct, decryptor, context);

    // 각 행을 왼쪽으로 1 칸 회전
    Ciphertext ct_rot;
    evaluator.rotate_rows(ct, 1, galois_keys, ct_rot);
    print_rows("rotate_rows(1)", ct_rot, decryptor, encoder);
    print_ct("rotate_rows(1)", ct_rot, decryptor, context);

    // 두 행을 맞바꾸기
    Ciphertext ct_swap;
    evaluator.rotate_columns(ct, galois_keys, ct_swap);
    print_rows("rotate_columns", ct_swap, decryptor, encoder);

    wait_enter("다음: 아무 칸 회전, 회전 키 전략 3가지");

    // ---------------------------------------------------------------
    // 2. 아무 칸 수 k 회전 (1 <= k < n/2 = 4096): 회전 키 전략 3 가지 비교
    // ---------------------------------------------------------------
    const int N = static_cast<int>(row); // 한 행의 슬롯 수 = 4096
    const int B = 64;                    // BSGS: baby step 1 ~ 63, giant step 64 x (1 ~ 63)

    // (a) 1 칸 회전 키 하나만
    Timer tm_key_a;
    GaloisKeys keys_one;
    keygen.create_galois_keys(vector<int>{ 1 }, keys_one);
    double key_ms_a = tm_key_a.ms();

    // (b) SEAL 기본 키 (2 의 거듭제곱) = 위에서 만든 galois_keys
    Timer tm_key_b;
    GaloisKeys keys_default;
    keygen.create_galois_keys(keys_default);
    double key_ms_b = tm_key_b.ms();

    // (c) BSGS 키: baby step 1 ~ B-1, giant step B x (1 ~ N/B - 1)
    Timer tm_key_c;
    vector<int> bsgs_steps;
    for (int b = 1; b < B; b++)
    {
        bsgs_steps.push_back(b);
    }
    for (int g = 1; g < N / B; g++)
    {
        bsgs_steps.push_back(B * g);
    }
    GaloisKeys keys_bsgs;
    keygen.create_galois_keys(bsgs_steps, keys_bsgs);
    double key_ms_c = tm_key_c.ms();

    // 전략별로 ct 를 왼쪽으로 k 칸 회전
    auto rotate_a = [&](Ciphertext &c, int k) {
        // 1 칸 회전을 k 번
        for (int i = 0; i < k; i++)
        {
            evaluator.rotate_rows_inplace(c, 1, keys_one);
        }
    };
    auto rotate_b = [&](Ciphertext &c, int k) {
        // SEAL 이 k 칸을 2 의 거듭제곱 칸 회전들로 나눠서 회전
        evaluator.rotate_rows_inplace(c, k, keys_default);
    };
    auto rotate_c = [&](Ciphertext &c, int k) {
        // k = B * g + b 로 나눠서 giant step 1 번 + baby step 1 번
        int giant = (k / B) * B;
        int baby = k % B;
        if (giant > 0)
        {
            evaluator.rotate_rows_inplace(c, giant, keys_bsgs);
        }
        if (baby > 0)
        {
            evaluator.rotate_rows_inplace(c, baby, keys_bsgs);
        }
    };

    cout << "\n" << left << setw(20) << "strategy" << right << setw(6) << "keys" << setw(10) << "key MB" << setw(14)
         << "keygen ms" << "\n";
    auto key_row = [&](const string &name, const GaloisKeys &keys, double ms) {
        cout << left << setw(20) << name << right << setw(6) << keys.size() << fixed << setprecision(1) << setw(10)
             << key_mb(keys) << setw(14) << ms << "\n";
        cout.unsetf(ios::fixed);
    };
    key_row("(a) one key", keys_one, key_ms_a);
    key_row("(b) default (2^i)", keys_default, key_ms_b);
    key_row("(c) BSGS", keys_bsgs, key_ms_c);

    // 이론값: 각 전략의 key switching 횟수
    //   (a) k 번
    //   (b) k 를 2 의 거듭제곱들의 합/차로 쪼갠 개수 (이진법처럼 쪼개되 빼기도 씀, k 가 N/2 보다 크면 N - k 칸 반대로)
    //   (c) giant 1 번 + baby 1 번 (최대 2 번)
    auto switches_b = [&](int k) {
        int x = (k > N / 2) ? N - k : k;
        int w = 0;
        while (x > 0)
        {
            if (x & 1)
            {
                x -= (x & 2) ? -1 : 1; // ...11 이면 +1 (빼기), ...01 이면 -1
                w++;
            }
            x >>= 1;
        }
        return w;
    };

    cout << "\n" << right << setw(6) << "k" << setw(22) << "(a) switch / ms" << setw(22) << "(b) switch / ms"
         << setw(22) << "(c) switch / ms" << "\n";
    double sum_a = 0, sum_b = 0, sum_c = 0;
    vector<int> ks = { 1, 183, 1000, 2047, 3000, 4095 };
    for (int k : ks)
    {
        vector<uint64_t> expected(n);
        for (size_t r = 0; r < 2; r++)
        {
            for (size_t i = 0; i < row; i++)
            {
                expected[r * row + i] = m[r * row + (i + k) % row];
            }
        }
        auto measure = [&](auto rotate, double &sum) {
            Ciphertext c = ct;
            Timer tm;
            rotate(c, k);
            double ms = tm.ms();
            sum += ms;
            Plaintext p;
            vector<uint64_t> v;
            decryptor.decrypt(c, p);
            encoder.decode(p, v);
            return make_pair(ms, v == expected);
        };
        auto [ms_a, ok_a] = measure(rotate_a, sum_a);
        auto [ms_b, ok_b] = measure(rotate_b, sum_b);
        auto [ms_c, ok_c] = measure(rotate_c, sum_c);
        int sw_c = (k / B > 0) + (k % B > 0);
        auto cell = [](int sw, double ms, bool ok) {
            ostringstream o;
            o << fixed << setprecision(2) << sw << " / " << ms << (ok ? "" : " (X)");
            return o.str();
        };
        cout << setw(6) << k << setw(22) << cell(k, ms_a, ok_a) << setw(22) << cell(switches_b(k), ms_b, ok_b)
             << setw(22) << cell(sw_c, ms_c, ok_c) << "\n";
    }
    cout << fixed << setprecision(2) << setw(6) << "avg" << setw(22) << sum_a / ks.size() << setw(22)
         << sum_b / ks.size() << setw(22) << sum_c / ks.size() << "\n";
    cout.unsetf(ios::fixed);
    cout << "(X) = 회전 결과가 틀림\n";

    return 0;
}
