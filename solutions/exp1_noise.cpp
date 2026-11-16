//   빌드: cmake --build build --target exp1_noise
//   실행: ./build/exp1_noise
#include <cmath>
#include <random>
#include "common.h"

using namespace std;
using namespace seal;

int main()
{
    // t 의 비트 수를 20, 40 으로 바꿔 가며 실행
    int t_bits = 20;

    EncryptionParameters parms(scheme_type::bgv);
    size_t n = 8192;
    parms.set_poly_modulus_degree(n);
    parms.set_coeff_modulus(CoeffModulus::BFVDefault(n));
    parms.set_plain_modulus(PlainModulus::Batching(n, t_bits));

    SEALContext context(parms);
    print_params(context);
    wait_enter("다음: 연산별 노이즈 예산");
    uint64_t t = parms.plain_modulus().value();

    KeyGenerator keygen(context);
    SecretKey secret_key = keygen.secret_key();
    PublicKey public_key;
    keygen.create_public_key(public_key);
    RelinKeys relin_keys;
    keygen.create_relin_keys(relin_keys);

    Encryptor encryptor(context, public_key);
    Decryptor decryptor(context, secret_key);
    Evaluator evaluator(context);
    BatchEncoder encoder(context);

    // 0 이상 t 미만의 랜덤 벡터 a, b 생성
    mt19937_64 rng(1);
    vector<uint64_t> a(encoder.slot_count()), b(encoder.slot_count());
    for (size_t i = 0; i < a.size(); i++)
    {
        a[i] = rng() % t;
        b[i] = rng() % t;
    }

    // 평문벡터 인코딩 & 암호화
    Plaintext pt_a, pt_b;
    encoder.encode(a, pt_a);
    encoder.encode(b, pt_b);
    Ciphertext ct_a, ct_b;
    encryptor.encrypt(pt_a, ct_a);
    encryptor.encrypt(pt_b, ct_b);

    // fresh 암호문의 예산 확인 (b_fresh = fresh 암호문의 예산)    
    int b_fresh = print_ct("fresh", ct_a, decryptor, context);

    // 암호문 + 암호문
    Ciphertext ct_add;
    evaluator.add(ct_a, ct_b, ct_add);
    int b_add = print_ct("ct + ct", ct_add, decryptor, context);

    // 암호문 x 평문 (랜덤 벡터 b와 곱)
    Ciphertext ct_mul_pt;
    evaluator.multiply_plain(ct_a, pt_b, ct_mul_pt);
    int b_mulpt = print_ct("ct x pt (b)", ct_mul_pt, decryptor, context);

    // 암호문 x 암호문
    Ciphertext ct_mul;
    evaluator.multiply(ct_a, ct_b, ct_mul);
    int b_mul = print_ct("ct x ct", ct_mul, decryptor, context);

    // Relinearize 
    evaluator.relinearize_inplace(ct_mul, relin_keys);
    int b_relin = print_ct("relinearize", ct_mul, decryptor, context);

    // 한 번 더 곱해보기: (a * b)^2
    Ciphertext ct_sq;
    evaluator.multiply(ct_mul, ct_mul, ct_sq);
    evaluator.relinearize_inplace(ct_sq, relin_keys);
    int b_sq = print_ct("(ct x ct)^2", ct_sq, decryptor, context);

    wait_enter("다음: 복호화 확인과 이론값 비교");

    // 복호화 결과 확인
    vector<uint64_t> ab(a.size()), ab2(a.size());
    for (size_t i = 0; i < a.size(); i++)
    {
        ab[i] = mulmod(a[i], b[i], t);
        ab2[i] = mulmod(ab[i], ab[i], t);
    }
    cout << "\n";
    print_check("a * b", ct_mul, ab, decryptor, encoder);
    print_check("(a * b)^2", ct_sq, ab2, decryptor, encoder);

    // 이론과 비교: 예산 감소량 = 에러 비트 증가량
    //  - log_q: 암호문 모듈러스 q 의 비트 수 (special prime 제외)
    //  - e0: fresh 암호문의 에러 비트 수
    //  - e1: 첫 곱셈 (relinearize) 후의 에러 비트 수

    //  - 강의 식의 log n 은 worst-case (n 개 항이 같은 부호로 쌓임). 실제로는 부호가 랜덤이라 sqrt(n) 배 -> log n / 2 를 쓴다
    double log_n = log2(n);
    double log_t = log2(t);
    double log_q = context.first_context_data()->total_coeff_modulus_bit_count();
    double e0 = log_q - log_t - b_fresh;
    double e1 = log_q - log_t - b_relin;
    cout << "\n";
    print_compare("ct + ct", 1, b_fresh - b_add);
    print_compare("ct x pt (b)", log_t + log_n / 2, b_fresh - b_mulpt);
    print_compare("ct x ct", e0 + log_t + log_n / 2, b_fresh - b_mul);
    print_compare("(ct x ct)^2", e1 + log_t + log_n / 2, b_relin - b_sq);

    return 0;
}
