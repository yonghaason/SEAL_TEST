//   빌드: cmake --build build --target exp8_psi_powers
//   실행: ./build/exp8_psi_powers
//
// 거듭제곱을 더 보내기.
//   실험 7 에서는 받는 쪽이 Enc(y) 하나만 보내서, 보내는 쪽이 y^2 ~ y^B 를 모두 곱셈으로 만들었다 (깊이 log B).
//   받는 쪽은 y 를 알고 있으니 y^2, y^4, ... 를 평문으로 계산해서 같이 암호화해 보낼 수 있다.
//   -> 보내는 암호문 (통신) 은 늘고, 보내는 쪽의 곱셈 (계산, 깊이) 은 준다.
#include <cmath>
#include <random>
#include "psi_common.h"

using namespace std;
using namespace seal;

// powers 에는 받는 쪽이 보낸 거듭제곱만 들어 있다 (나머지는 빈 암호문).
// plan.split[k] = (a, b) 이면 y^k = y^a * y^b 로 만든다. (0, 0) 이면 받은 것이라 그대로 둔다.
// plan 은 작은 k 부터 만들 수 있도록 짜여 있으므로 k = 1, 2, ..., D 순서로 채우면 된다.
void compute_powers(vector<Ciphertext> &powers, const PowerPlan &plan, Evaluator &evaluator,
                    const RelinKeys &relin_keys)
{
    int D = static_cast<int>(plan.split.size()) - 1;
    for (int k = 1; k <= D; k++)
    {
        auto [a, b] = plan.split[k];
        if (a == 0)
        {
            continue; // 받은 거듭제곱
        }
        // y^a x y^b -> y^k, 그리고 relinearize
        evaluator.multiply(powers[a], powers[b], powers[k]);
        evaluator.relinearize_inplace(powers[k], relin_keys);
    }
}

// r * p(y) 계산. 실험 7 에서 쓴 함수와 같다 (채워서 준다).
Ciphertext evaluate(const vector<Ciphertext> &powers, const vector<uint64_t> &coeffs, const Plaintext &pt_r,
                    BatchEncoder &encoder, Evaluator &evaluator)
{
    int B = static_cast<int>(coeffs.size()) - 1;
    size_t slots = encoder.slot_count();
    Ciphertext result;
    for (int k = 1; k <= B; k++)
    {
        Plaintext pt_c;
        encoder.encode(vector<uint64_t>(slots, coeffs[k]), pt_c);
        Ciphertext term;
        evaluator.multiply_plain(powers[k], pt_c, term);
        if (k == 1)
        {
            result = term;
        }
        else
        {
            evaluator.add_inplace(result, term);
        }
    }
    Plaintext pt_c0;
    encoder.encode(vector<uint64_t>(slots, coeffs[0]), pt_c0);
    evaluator.add_plain_inplace(result, pt_c0);
    evaluator.multiply_plain_inplace(result, pt_r);
    return result;
}

int main()
{
    // 실험 7 과 같은 파라미터
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

    // 집합 크기 B = 16: 실험 7 에서 y 하나만 보내면 실패한 크기
    const int B = 16;
    mt19937_64 rng(1);
    unordered_set<uint64_t> used;
    vector<uint64_t> X = random_items(B, t, rng, used);
    vector<uint64_t> Y = random_items(slots, t, rng, used);
    vector<size_t> match_slots = { 100, 5000 };
    Y[match_slots[0]] = X[0];
    Y[match_slots[1]] = X[1];
    vector<uint64_t> coeffs = poly_from_roots(X, t);

    vector<uint64_t> r(slots);
    for (auto &v : r)
    {
        v = 1 + rng() % (t - 1);
    }
    Plaintext pt_r;
    encoder.encode(r, pt_r);

    // 받는 쪽이 보낼 거듭제곱 (지수 목록) 4 가지
    vector<string> names = { "(a) y only", "(b) powers of 2", "(c) all powers" };
    vector<vector<int>> strategies(3);

    // (a) y 하나만 보내기: { 1 }
    strategies[0] = { 1 };

    // (b) 2 의 거듭제곱 보내기: { 1, 2, 4, 8 }  (B 보다 작은 2 의 거듭제곱)
    for (int k = 1; k < B; k *= 2)
    {
        strategies[1].push_back(k);
    }

    // (c) 모두 보내기: { 1, 2, ..., B }
    for (int k = 1; k <= B; k++)
    {
        strategies[2].push_back(k);
    }


    wait_enter("다음: 거듭제곱을 보내는 전략 3 가지 (B = 16)");

    cout << "\n" << left << setw(18) << "strategy" << right << setw(6) << "sent" << setw(10) << "upload" << setw(8)
         << "mults" << setw(7) << "depth" << setw(8) << "budget" << setw(8) << "result" << setw(12) << "server"
         << "\n";
    for (int s = 0; s < 3; s++)
    {
        const vector<int> &sent = strategies[s];
        if (sent.empty())
        {
            cerr << "\n[오류] '" << names[s] << "' 의 보낼 거듭제곱 목록이 비어 있습니다.\n";
            exit(1);
        }

        // 받는 쪽: y^k 를 평문으로 계산해서 각각 암호화 (채워서 준다)
        vector<Ciphertext> powers(B + 1);
        double upload_mb = 0;
        for (int k : sent)
        {
            vector<uint64_t> yk(slots);
            for (size_t j = 0; j < slots; j++)
            {
                yk[j] = powmod(Y[j], k, t);
            }
            Plaintext pt;
            encoder.encode(yk, pt);
            encryptor.encrypt(pt, powers[k]);
            upload_mb += powers[k].save_size(compr_mode_type::none) / 1e6;
        }

        // 보내는 쪽: 나머지 거듭제곱을 곱셈으로 만들고 r * p(y) 계산
        PowerPlan plan = plan_powers(sent, B);
        Timer tm;
        compute_powers(powers, plan, evaluator, relin_keys);
        for (int k = 1; k <= B; k++)
        {
            require_ct("y^" + to_string(k), powers[k]);
        }
        Ciphertext result = evaluate(powers, coeffs, pt_r, encoder, evaluator);
        double ms = tm.ms();

        // 받는 쪽: 복호화해서 확인
        int budget = decryptor.invariant_noise_budget(result);
        Plaintext pt_res;
        decryptor.decrypt(result, pt_res);
        vector<uint64_t> res;
        encoder.decode(pt_res, res);
        bool ok = true;
        for (size_t j = 0; j < slots; j++)
        {
            bool expected = (j == match_slots[0] || j == match_slots[1]);
            ok = ok && ((res[j] == 0) == expected);
        }

        cout << left << setw(18) << names[s] << right << setw(6) << sent.size() << fixed << setprecision(1)
             << setw(7) << upload_mb << " MB" << setw(8) << plan.mult_count << setw(7) << plan.max_depth << setw(8)
             << budget << setw(8) << (ok ? "OK" : "FAIL") << setw(9) << ms << " ms" << "\n";
        cout.unsetf(ios::fixed);
    }

    return 0;
}
