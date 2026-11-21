//   빌드: cmake --build build --target exp7_psi_basic
//   실행: ./build/exp7_psi_basic
//
// 가장 단순한 HE 기반 PSI.
//   받는 쪽 (receiver): 집합 Y 를 슬롯마다 하나씩 넣어 암호화해 보낸다 (슬롯 8192 개).
//   보내는 쪽 (sender): 집합 X (크기 B) 로 p(y) = prod_{x in X} (y - x) 를 만든다.
//                      p(y) = c_0 + c_1 y + ... + c_B y^B  의 계수는 평문으로 미리 계산한다.
//                      암호문 상태로 r * p(y) 를 계산해 돌려준다 (r: 슬롯마다 0 이 아닌 랜덤).
//   받는 쪽: 복호화해서 0 인 슬롯 = y 가 X 에 있음.
#include <cmath>
#include <random>
#include "psi_common.h"

using namespace std;
using namespace seal;

// powers[1] = Enc(y) 가 들어 있을 때 powers[2] ~ powers[B] 에 Enc(y^k) 를 채운다.
// y^k = y^(k/2) * y^(k - k/2) 로 만들면 곱셈 깊이가 ceil(log2 k) 로 가장 얕다.
void compute_powers(vector<Ciphertext> &powers, int B, Evaluator &evaluator, const RelinKeys &relin_keys)
{
    for (int k = 2; k <= B; k++)
    {
        // TODO: powers[k / 2] x powers[k - k / 2] -> powers[k],  그리고 relinearize

    }
}

// r * p(y) = r * (c_0 + c_1 y + ... + c_B y^B) 를 계산한다.
//   coeffs[k] = c_k (모든 슬롯에 같은 다항식).  pt_r = 슬롯마다 다른 랜덤 r 을 인코딩한 평문.
Ciphertext evaluate(const vector<Ciphertext> &powers, const vector<uint64_t> &coeffs, const Plaintext &pt_r,
                    BatchEncoder &encoder, Evaluator &evaluator)
{
    int B = static_cast<int>(coeffs.size()) - 1;
    size_t slots = encoder.slot_count();
    Ciphertext result;

    // c_1 y + ... + c_B y^B: 계수를 모든 슬롯에 넣어 인코딩하고, y^k 에 곱해서 더한다
    for (int k = 1; k <= B; k++)
    {
        Plaintext pt_c;
        Ciphertext term;
        // TODO: coeffs[k] 를 모든 슬롯에 넣어 인코딩 -> pt_c:  encoder.encode(vector<uint64_t>(slots, coeffs[k]), pt_c)
        //       powers[k] x pt_c -> term (multiply_plain)
        //       result 에 term 더하기 (k == 1 이면 대입)

    }

    // + c_0
    Plaintext pt_c0;
    // TODO: coeffs[0] 을 모든 슬롯에 넣어 인코딩 -> pt_c0,  result 에 더하기 (add_plain_inplace)


    // r 곱하기: p(y) = 0 이면 그대로 0, 아니면 랜덤 값이 되어 X 에 대한 정보가 새지 않는다
    // TODO: result x pt_r (multiply_plain_inplace)


    return result;
}

int main()
{
    // 실험 0 과 같은 크기 (n = 8192, q = BFVDefault) 의 BFV. BFV 는 곱셈 한 번의 예산 감소가 일정하다 (실험 2).
    EncryptionParameters parms(scheme_type::bfv);
    size_t n = 8192;
    parms.set_poly_modulus_degree(n);
    parms.set_coeff_modulus(CoeffModulus::BFVDefault(n));
    parms.set_plain_modulus(PlainModulus::Batching(n, 20));

    SEALContext context(parms);
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
    size_t slots = encoder.slot_count();

    // 보내는 쪽 원소 16 개를 먼저 뽑는다. 집합 크기 B 로 실행할 때는 앞의 B 개가 X 이다.
    // 받는 쪽 Y (8192 개) 에는 X 의 앞 두 원소가 들어 있다 -> 교집합은 항상 2 개.
    mt19937_64 rng(1);
    unordered_set<uint64_t> used;
    vector<uint64_t> X_all = random_items(16, t, rng, used);
    vector<uint64_t> Y = random_items(slots, t, rng, used);
    vector<size_t> match_slots = { 100, 5000 };
    Y[match_slots[0]] = X_all[0];
    Y[match_slots[1]] = X_all[1];

    // 받는 쪽: Y 를 암호화
    Plaintext pt_y;
    encoder.encode(Y, pt_y);
    Ciphertext ct_y;
    encryptor.encrypt(pt_y, ct_y);
    int b_fresh = print_ct("fresh y", ct_y, decryptor, context);

    // 보내는 쪽의 마스크 r (0 이 아닌 랜덤)
    vector<uint64_t> r(slots);
    for (auto &v : r)
    {
        v = 1 + rng() % (t - 1);
    }
    Plaintext pt_r;
    encoder.encode(r, pt_r);

    wait_enter("다음: 집합 크기 B = 2, 4, 8, 16");

    // 이론: BFV 곱셈 한 번의 예산 감소 = log t + 2 (log n / 2)  (실험 2)
    //       y^B 를 만드는 곱셈 깊이 = ceil(log2 B)  ->  y^B 의 예산 = fresh - 깊이 x (곱셈 한 번)
    double log_t = log2(t);
    double mult_cost = log_t + 2 * (log2(n) / 2);

    cout << "\n" << setw(4) << "B" << setw(7) << "depth" << setw(22) << "budget y^B (th/meas)" << setw(14)
         << "final budget" << setw(10) << "found" << setw(8) << "wrong" << setw(12) << "time" << "\n";
    for (int B : { 2, 4, 8, 16 })
    {
        vector<uint64_t> X(X_all.begin(), X_all.begin() + B);
        vector<uint64_t> coeffs = poly_from_roots(X, t);

        Timer tm;
        vector<Ciphertext> powers(B + 1);
        powers[1] = ct_y;
        compute_powers(powers, B, evaluator, relin_keys);
        Ciphertext result = evaluate(powers, coeffs, pt_r, encoder, evaluator);
        double ms = tm.ms();

        require_ct("y^B", powers[B]);
        require_ct("r * p(y)", result);
        int depth = static_cast<int>(ceil(log2(B)));
        int b_pow = decryptor.invariant_noise_budget(powers[B]);
        int b_final = decryptor.invariant_noise_budget(result);

        // 받는 쪽: 복호화해서 0 인 슬롯 세기
        Plaintext pt_res;
        decryptor.decrypt(result, pt_res);
        vector<uint64_t> res;
        encoder.decode(pt_res, res);
        int found = 0, wrong = 0;
        for (size_t j = 0; j < slots; j++)
        {
            bool expected = (j == match_slots[0] || j == match_slots[1]);
            if (res[j] == 0 && expected)
            {
                found++;
            }
            else if (res[j] == 0 || expected)
            {
                wrong++;
            }
        }

        ostringstream th;
        th << fixed << setprecision(0) << b_fresh - depth * mult_cost << " / " << b_pow;
        cout << setw(4) << B << setw(7) << depth << setw(22) << th.str() << setw(14) << b_final << setw(8) << found
             << " /2" << setw(8) << wrong << fixed << setprecision(1) << setw(9) << ms << " ms" << "\n";
        cout.unsetf(ios::fixed);
    }
    cout << "(wrong = 0 이 아니어야 할 슬롯이 0 이거나, 0 이어야 할 슬롯이 0 이 아닌 수)\n";

    return 0;
}
