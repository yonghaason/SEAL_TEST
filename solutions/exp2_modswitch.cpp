//   빌드: cmake --build build --target exp2_modswitch
//   실행: ./build/exp2_modswitch
#include <cmath>
#include <random>
#include "common.h"

using namespace std;
using namespace seal;

// n = 16384, q = 36비트 RNS 소수 7개, special prime 60비트, t = 20비트
EncryptionParameters make_parms(scheme_type scheme)
{
    EncryptionParameters parms(scheme);
    size_t n = 16384;
    parms.set_poly_modulus_degree(n);
    parms.set_coeff_modulus(CoeffModulus::Create(n, { 36, 36, 36, 36, 36, 36, 36, 60 }));
    parms.set_plain_modulus(PlainModulus::Batching(n, 20));
    return parms;
}

// 현재 레벨에서 "제곱 + relinearize" 1 회에 걸리는 평균 시간 (ms). 
double time_mult(Evaluator &evaluator, const Ciphertext &ct, const RelinKeys &relin_keys, int reps = 20)
{
    Timer tm;
    for (int r = 0; r < reps; r++)
    {
        Ciphertext tmp;
        evaluator.square(ct, tmp);
        evaluator.relinearize_inplace(tmp, relin_keys);
    }
    return tm.ms() / reps;
}

// 같은 암호문을 4번 제곱. use_modswitch = true 이면 곱셈마다 mod switch.
void run(scheme_type scheme, bool use_modswitch)
{
    EncryptionParameters parms = make_parms(scheme);
    SEALContext context(parms);
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

    cout << "\n=== " << (scheme == scheme_type::bgv ? "BGV" : "BFV") << ", mod switch "
         << (use_modswitch ? "O" : "X") << " ===\n";

    mt19937_64 rng(1);
    vector<uint64_t> a(encoder.slot_count());
    for (auto &x : a)
    {
        x = rng() % t;
    }
    // 평문벡터 인코딩 & 암호화
    Plaintext pt;
    encoder.encode(a, pt);
    Ciphertext ct;
    encryptor.encrypt(pt, ct);
    print_ct("fresh", ct, decryptor, context);

    double log_n = log2(parms.poly_modulus_degree());
    double log_t = log2(t);
    vector<uint64_t> expected = a;
    double total_ms = 0;

    for (int i = 1; i <= 4; i++)
    {
        // 곱하기 전의 에러 비트 수: (현재 레벨의 log q) - log t - 예산
        int b_before = decryptor.invariant_noise_budget(ct);
        double log_q = context.get_context_data(ct.parms_id())->total_coeff_modulus_bit_count();
        double e = log_q - log_t - b_before;
        double ms = time_mult(evaluator, ct, relin_keys);
        total_ms += ms;

        // 암호문 제곱 후 relinearize
        evaluator.square_inplace(ct);
        evaluator.relinearize_inplace(ct, relin_keys);
        int b_after = print_ct("mult " + to_string(i), ct, decryptor, context);
        for (auto &x : expected)
        {
            x = mulmod(x, x, t);
        }

        // 이론값: 곱셈 1회의 예산 감소량 (BGV / BFV)
        //   강의 식의 log n 은 worst-case. 실제로는 sqrt(n) 배라 log n / 2 로 바꿔 쓴다 (BFV 는 log n 이 두 번)
        double theory = (scheme == scheme_type::bgv) ? e + log_t + log_n / 2 : log_t + 2 * (log_n / 2);
        print_compare("  (budget drop)", theory, b_before - b_after);
        cout << fixed << setprecision(2) << "  (time) " << ms << " ms\n";
        cout.unsetf(ios::fixed);

        if (b_after == 0)
        {
            break;
        }
        if (use_modswitch)
        {
            // mod switch 로 다음 레벨로 내리기
            evaluator.mod_switch_to_next_inplace(ct);
            print_ct("mod switch", ct, decryptor, context);
        }
    }
    print_check("result", ct, expected, decryptor, encoder);
    cout << fixed << setprecision(2) << "total mult time: " << total_ms << " ms\n";
    cout.unsetf(ios::fixed);
}

int main()
{
    SEALContext context(make_parms(scheme_type::bgv));
    print_params(context);
    wait_enter("다음: BGV, mod switch X");

    run(scheme_type::bgv, false);
    wait_enter("다음: BGV, mod switch O");
    run(scheme_type::bgv, true);
    wait_enter("다음: BFV, mod switch X");
    run(scheme_type::bfv, false);
    wait_enter("다음: BFV, mod switch O");
    run(scheme_type::bfv, true);

    return 0;
}
