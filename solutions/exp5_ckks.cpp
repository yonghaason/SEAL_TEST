//   빌드: cmake --build build --target exp5_ckks
//   실행: ./build/exp5_ckks
#include <cmath>
#include <random>
#include "common.h"

using namespace std;
using namespace seal;

// 두 암호문을 더해 보고, SEAL 이 예외를 던지면 그 메시지를 출력한다.
void try_add(const string &label, Evaluator &evaluator, const Ciphertext &ct1, const Ciphertext &ct2)
{
    require_ct(label, ct1);
    require_ct(label, ct2);
    try
    {
        Ciphertext tmp;
        evaluator.add(ct1, ct2, tmp);
        cout << left << setw(24) << label << right << " add OK\n";
    }
    catch (const exception &e)
    {
        cout << left << setw(24) << label << right << " add FAIL: " << e.what() << "\n";
    }
}

int main()
{
    // scale (Delta) 의 비트 수. 40, 30 으로 바꿔 가며 실행
    int scale_bits = 40;
    double scale = pow(2.0, scale_bits);

    // q = { 60, scale_bits, scale_bits } + special prime 60
    //  - rescale 은 q 의 마지막 RNS 소수로 나눈다. 그 소수가 Delta 와 비슷해야 scale 이 Delta 근처로 돌아온다
    //  - 곱셈 깊이 2 까지 쓰므로 Delta 크기의 소수가 2 개
    EncryptionParameters parms(scheme_type::ckks);
    size_t n = 8192;
    parms.set_poly_modulus_degree(n);
    parms.set_coeff_modulus(CoeffModulus::Create(n, { 60, scale_bits, scale_bits, 60 }));

    SEALContext context(parms);
    print_params(context);
    wait_enter("다음: 인코딩과 암호화");

    KeyGenerator keygen(context);
    SecretKey secret_key = keygen.secret_key();
    PublicKey public_key;
    keygen.create_public_key(public_key);
    RelinKeys relin_keys;
    keygen.create_relin_keys(relin_keys);

    Encryptor encryptor(context, public_key);
    Decryptor decryptor(context, secret_key);
    Evaluator evaluator(context);
    CKKSEncoder encoder(context);

    // -1 이상 1 이하의 랜덤 실수 벡터 x (슬롯은 n / 2 개)
    // 비교용 평문 계산: x^2, x^2 + x, pi x^3 + 0.4 x + 1
    const double PI = 3.14159265358979;
    mt19937_64 rng(1);
    uniform_real_distribution<double> dist(-1.0, 1.0);
    vector<double> x(encoder.slot_count()), x2(x.size()), x2_plus_x(x.size()), poly(x.size());
    for (size_t i = 0; i < x.size(); i++)
    {
        x[i] = dist(rng);
        x2[i] = x[i] * x[i];
        x2_plus_x[i] = x2[i] + x[i];
        poly[i] = PI * x[i] * x[i] * x[i] + 0.4 * x[i] + 1.0;
    }

    // ---------------------------------------------------------------
    // 1. 인코딩 & 암호화: BFV/BGV 와 달리 encode 에 scale 을 준다
    // ---------------------------------------------------------------
    Plaintext pt_x;
    encoder.encode(x, scale, pt_x);
    Ciphertext ct_x;
    encryptor.encrypt(pt_x, ct_x);

    print_ct_ckks("fresh", ct_x, context);
    double p_fresh = print_error("x", ct_x, x, decryptor, encoder);

    wait_enter("다음: 곱셈과 rescale");

    // ---------------------------------------------------------------
    // 2. x^2: 곱셈 -> relinearize -> rescale
    // ---------------------------------------------------------------
    // 암호문 x 암호문, 그리고 relinearize
    Ciphertext ct_x2;
    evaluator.multiply(ct_x, ct_x, ct_x2);
    evaluator.relinearize_inplace(ct_x2, relin_keys);
    double s_mul = print_ct_ckks("x * x", ct_x2, context);

    // rescale: scale 이 Delta^2 -> Delta 근처로, 레벨은 1 내려간다
    evaluator.rescale_to_next_inplace(ct_x2);
    double s_rescale = print_ct_ckks("rescale", ct_x2, context);
    double p_x2 = print_error("x^2", ct_x2, x2, decryptor, encoder);

    wait_enter("다음: x^2 + x (레벨과 scale 맞추기)");

    // ---------------------------------------------------------------
    // 3. x^2 + x: 더하려면 두 암호문의 레벨과 scale 이 같아야 한다
    // ---------------------------------------------------------------
    // 그냥 더해 보기 -> 레벨이 달라서 실패
    try_add("x^2 + x", evaluator, ct_x2, ct_x);

    // ct_x 를 ct_x2 와 같은 레벨로 내리기
    Ciphertext ct_x_low;
    evaluator.mod_switch_to(ct_x, ct_x2.parms_id(), ct_x_low);

    // 다시 더해 보기 -> 이번에는 scale 이 달라서 실패
    try_add("x^2 + x (same level)", evaluator, ct_x2, ct_x_low);
    cout << fixed << setprecision(0) << "  scale of x^2 = " << ct_x2.scale() << "\n"
         << "  scale of x   = " << ct_x_low.scale() << "\n";
    cout.unsetf(ios::fixed);

    // ct_x2 의 scale 을 Delta 로 맞추기 (거의 같으므로 같다고 적어 둔다)
    ct_x2.scale() = scale;

    // 이제 더하기
    Ciphertext ct_sum;
    evaluator.add(ct_x2, ct_x_low, ct_sum);
    print_ct_ckks("x^2 + x", ct_sum, context);
    double p_sum = print_error("x^2 + x", ct_sum, x2_plus_x, decryptor, encoder);

    wait_enter("다음: pi x^3 + 0.4 x + 1");

    // ---------------------------------------------------------------
    // 4. pi x^3 + 0.4 x + 1: 곱셈 깊이 2 (x^3 = x^2 * (pi x))
    // ---------------------------------------------------------------
    // pi x: 상수 pi 를 인코딩해서 ct_x 에 곱하고 rescale
    Plaintext pt_pi;
    encoder.encode(PI, scale, pt_pi);
    Ciphertext ct_pix;
    evaluator.multiply_plain(ct_x, pt_pi, ct_pix);
    evaluator.rescale_to_next_inplace(ct_pix);
    print_ct_ckks("pi x", ct_pix, context);

    // pi x^3 = x^2 * (pi x): 곱셈 -> relinearize -> rescale
    Ciphertext ct_pix3;
    evaluator.multiply(ct_x2, ct_pix, ct_pix3);
    evaluator.relinearize_inplace(ct_pix3, relin_keys);
    evaluator.rescale_to_next_inplace(ct_pix3);
    print_ct_ckks("pi x^3", ct_pix3, context);

    // 0.4 x: 상수 0.4 를 곱하고 rescale, 그리고 ct_pix3 와 같은 레벨로 내리기
    Plaintext pt_04;
    encoder.encode(0.4, scale, pt_04);
    Ciphertext ct_04x;
    evaluator.multiply_plain(ct_x, pt_04, ct_04x);
    evaluator.rescale_to_next_inplace(ct_04x);
    evaluator.mod_switch_to_inplace(ct_04x, ct_pix3.parms_id());
    print_ct_ckks("0.4 x", ct_04x, context);

    // scale 을 Delta 로 맞추고 더하기. 상수 1 은 같은 레벨로 인코딩해서 add_plain
    ct_pix3.scale() = scale;
    ct_04x.scale() = scale;
    Ciphertext ct_poly;
    evaluator.add(ct_pix3, ct_04x, ct_poly);
    Plaintext pt_one;
    encoder.encode(1.0, ct_poly.parms_id(), scale, pt_one);
    evaluator.add_plain_inplace(ct_poly, pt_one);
    print_ct_ckks("pi x^3 + 0.4 x + 1", ct_poly, context);
    double p_poly = print_error("pi x^3 + 0.4 x + 1", ct_poly, poly, decryptor, encoder);

    wait_enter("다음: 이론값 비교");

    // 이론과 비교
    //  - scale: 곱하면 Delta^2, rescale 하면 Delta^2 / (버린 RNS 소수)
    //  - 정밀도 (비트) = -log2(최대 오차)
    //  - fresh: 에러의 표준편차는 n / 6 이고 (Delta 와 무관), 슬롯 n / 2 개 중 최댓값은 그 6배쯤 (+2.6 비트)
    //           오차 = 에러 / Delta 이므로 정밀도 = log2(Delta) - log2(n / 6) - 2.6
    //  - x^2: (x + e)^2 = x^2 + 2 x e 이므로 에러가 최대 2배 (-1 비트)
    //  - x^2 + x, 다항식: scale 을 Delta 로 고쳐 적은 만큼 값이 틀어진다
    //      d1, d0 = Delta / (rescale 에서 버린 RNS 소수) - 1  (소수가 Delta 와 어긋난 비율. d1 이 먼저 버린 소수)
    //      x^2 과 0.4 x 는 (1 + d1) 배, pi x^3 은 (1 + d1)^2 (1 + d0) 배가 된다
    double q1 = parms.coeff_modulus()[2].value();
    double q0 = parms.coeff_modulus()[1].value();
    double d1 = abs(scale / q1 - 1);
    double d0 = abs(scale / q0 - 1);
    cout << "\n";
    print_compare("scale: x * x", 2.0 * scale_bits, s_mul);
    print_compare("scale: rescale", 2.0 * scale_bits - log2(q1), s_rescale);
    print_compare("prec: fresh", scale_bits - log2(n / 6.0) - 2.6, p_fresh);
    print_compare("prec: x^2", p_fresh - 1, p_x2);
    print_compare("prec: x^2 + x", -log2(d1), p_sum);
    print_compare("prec: poly", -log2(PI * (2 * d1 + d0) + 0.4 * d1), p_poly);

    return 0;
}
