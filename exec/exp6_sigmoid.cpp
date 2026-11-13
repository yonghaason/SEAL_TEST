//   빌드: cmake --build build --target exp6_sigmoid
//   실행: ./build/exp6_sigmoid
#include <cmath>
#include "common.h"

using namespace std;
using namespace seal;

// 시그모이드와 3차 근사 다항식 (평문 계산)
double sigmoid(double z)
{
    return 1.0 / (1.0 + exp(-z));
}
double sigmoid3_plain(double z)
{
    return 0.5 + 0.197 * z - 0.004 * z * z * z;
}

// 암호문 ct_z 의 슬롯마다 0.5 + 0.197 z - 0.004 z^3 을 계산해서 돌려준다.
// 곱셈 깊이 2: z^3 항은 (z^2) * (-0.004 z) 로 만든다. 결과는 ct_z 보다 2 레벨 아래.
// scale 은 Delta. 상수는 ct_z 와 같은 레벨 (ct_z.parms_id()) 로 인코딩해야 곱할 수 있다.
Ciphertext sigmoid3(const Ciphertext &ct_z, Evaluator &evaluator, CKKSEncoder &encoder,
                    const RelinKeys &relin_keys, double scale)
{
    Ciphertext ct_out;
    // TODO: 아래 순서로 계산해서 ct_out 에 넣기
    //   1) z^2            : ct_z x ct_z -> relinearize -> rescale
    //   2) -0.004 z       : -0.004 를 인코딩 (encoder.encode(-0.004, ct_z.parms_id(), scale, pt)) 해서 곱하고 rescale
    //   3) -0.004 z^3     : 1) x 2) -> relinearize -> rescale
    //   4) 0.197 z        : 0.197 을 인코딩해서 곱하고 rescale, 그리고 3) 의 레벨로 mod switch
    //   5) 3), 4) 의 scale 에 scale 을 대입하고 더하기 -> ct_out
    //   6) 0.5 를 ct_out 의 레벨로 인코딩해서 더하기 (add_plain_inplace)

    return ct_out;
}

int main()
{
    // 실험 5 와 같은 파라미터: n = 8192, q = { 60, 40, 40 } + P 60, Delta = 2^40
    int scale_bits = 40;
    double scale = pow(2.0, scale_bits);

    EncryptionParameters parms(scheme_type::ckks);
    size_t n = 8192;
    parms.set_poly_modulus_degree(n);
    parms.set_coeff_modulus(CoeffModulus::Create(n, { 60, scale_bits, scale_bits, 60 }));

    SEALContext context(parms);
    print_params(context);

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

    // 입력 범위 [-range, range] 를 5, 8 로 바꿔 가며 계산한다
    vector<double> ranges = { 5.0, 8.0 };
    vector<double> err_approx, err_ckks, err_total;
    for (double range : ranges)
    {
        wait_enter("다음: z 의 범위 [-" + to_string(static_cast<int>(range)) + ", " +
                   to_string(static_cast<int>(range)) + "]");

        // z: -range 부터 range 까지 같은 간격으로 슬롯 n / 2 개
        size_t slots = encoder.slot_count();
        vector<double> z(slots), exact(slots), approx(slots);
        for (size_t i = 0; i < slots; i++)
        {
            z[i] = -range + 2.0 * range * i / (slots - 1);
            exact[i] = sigmoid(z[i]);
            approx[i] = sigmoid3_plain(z[i]);
        }

        // 인코딩 & 암호화
        Plaintext pt_z;
        encoder.encode(z, scale, pt_z);
        Ciphertext ct_z;
        encryptor.encrypt(pt_z, ct_z);

        // 암호문 상태에서 시그모이드 근사 계산
        Ciphertext ct_out = sigmoid3(ct_z, evaluator, encoder, relin_keys, scale);

        cout << "z in [" << -range << ", " << range << "]\n";
        print_ct_ckks("input z", ct_z, context);
        print_ct_ckks("sigmoid3(z)", ct_out, context);

        // 복호화해서 몇 개의 슬롯을 출력
        Plaintext pt_out;
        decryptor.decrypt(ct_out, pt_out);
        vector<double> got;
        encoder.decode(pt_out, got);
        cout << "\n" << setw(8) << "z" << setw(12) << "sigmoid" << setw(12) << "poly" << setw(12) << "CKKS" << "\n";
        cout << fixed << setprecision(4);
        for (size_t i : { size_t(0), slots / 4, slots / 2, 3 * slots / 4, slots - 1 })
        {
            cout << setw(8) << z[i] << setw(12) << exact[i] << setw(12) << approx[i] << setw(12) << got[i] << "\n";
        }
        cout.unsetf(ios::fixed);

        // 세 가지 오차의 최댓값
        //  - 근사 오차: |다항식 - 시그모이드|  (암호와 무관, 다항식 자체의 오차)
        //  - CKKS 오차: |복호화 결과 - 다항식|  (암호 계산에서 생긴 오차)
        //  - 전체 오차: |복호화 결과 - 시그모이드|
        double e_approx = 0, e_ckks = 0, e_total = 0;
        for (size_t i = 0; i < slots; i++)
        {
            e_approx = max(e_approx, abs(approx[i] - exact[i]));
            e_ckks = max(e_ckks, abs(got[i] - approx[i]));
            e_total = max(e_total, abs(got[i] - exact[i]));
        }
        err_approx.push_back(e_approx);
        err_ckks.push_back(e_ckks);
        err_total.push_back(e_total);
        cout << "\n";
    }

    wait_enter("다음: 오차 비교");

    cout << setw(10) << "range" << setw(16) << "approx error" << setw(16) << "CKKS error" << setw(16)
         << "total error" << "\n";
    cout << scientific << setprecision(2);
    for (size_t i = 0; i < ranges.size(); i++)
    {
        string label = "[-" + to_string(static_cast<int>(ranges[i])) + ", " +
                       to_string(static_cast<int>(ranges[i])) + "]";
        cout << setw(10) << label << setw(16) << err_approx[i] << setw(16) << err_ckks[i] << setw(16)
             << err_total[i] << "\n";
    }
    cout.unsetf(ios::floatfield);

    return 0;
}
