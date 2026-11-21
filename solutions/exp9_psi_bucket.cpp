//   빌드: cmake --build build --target exp9_psi_bucket
//   실행: ./build/exp9_psi_bucket
//
// 크기가 다른 두 집합의 PSI: 보내는 쪽 |X| = 2^18, 받는 쪽 |Y| = 4000, 교집합 100 개.
//   1. 해싱 (채워서 준다)
//      받는 쪽: cuckoo hashing 으로 Y 를 슬롯 8192 칸에 한 칸씩 넣는다.
//      보내는 쪽: simple hashing 으로 X 의 원소를 해시 함수 3 개가 가리키는 칸 모두에 넣는다 (칸 = bucket).
//      -> 슬롯 j 의 y 는 bucket j 의 원소들하고만 비교하면 된다. 슬롯마다 다른 다항식 p_j.
//   2. 파티션: bucket (최대 크기 Bmax) 을 ALPHA 조각으로 나눈다 -> 다항식 차수 D = Bmax / ALPHA.
//      조각마다 응답 암호문이 하나씩 생긴다.
//   3. 거듭제곱: 받는 쪽이 SEND 의 거듭제곱을 보내고, 보내는 쪽이 나머지 y^1 ~ y^D 를 만든다 (실험 8).
//
// 할 일: server_respond 를 채운 뒤, 맨 아래 ALPHA 와 SEND 를 바꿔 가며 실행해서 표를 채운다.
//   - 결과가 맞아야 한다 (found = 100, wrong = 0).
//   - 그중 통신량 (upload + download) 과 서버 시간이 작은 설정을 찾는다.
#include <cmath>
#include <random>
#include "psi_common.h"

using namespace std;
using namespace seal;

// ===== 바꿔 가며 실행할 값 =====
int ALPHA = 5;            // 파티션 수 (bucket 을 몇 조각으로 나눌지)
vector<int> SEND = { 1, 2, 4, 8, 16 }; // 받는 쪽이 보낼 거듭제곱의 지수 (1 은 꼭 넣는다)
// ===============================

// 보내는 쪽의 응답.
//   powers[k]: 받은 거듭제곱 Enc(y^k) (나머지는 빈 암호문, 크기 D + 1)
//   coeffs[p][k]: 파티션 p 의 다항식 계수 c_k 를 슬롯마다 넣어 인코딩한 평문 (슬롯 j 에는 bucket j 의 계수)
//   pt_r[p]: 파티션 p 의 마스크 r (슬롯마다 다른 랜덤)
// 반환값: 파티션마다 Enc(r * p(y)) 하나씩, ALPHA 개
vector<Ciphertext> server_respond(vector<Ciphertext> &powers, const PowerPlan &plan,
                                  const vector<vector<Plaintext>> &coeffs, const vector<Plaintext> &pt_r,
                                  Evaluator &evaluator, const RelinKeys &relin_keys, const SEALContext &context)
{
    int D = static_cast<int>(plan.split.size()) - 1;

    // (1) y^1 ~ y^D 만들기: 실험 8 의 compute_powers 와 같다
    for (int k = 1; k <= D; k++)
    {
        auto [a, b] = plan.split[k];
        if (a == 0)
        {
            continue;
        }
        evaluator.multiply(powers[a], powers[b], powers[k]);
        evaluator.relinearize_inplace(powers[k], relin_keys);
    }

    vector<Ciphertext> responses(coeffs.size());
    for (size_t p = 0; p < coeffs.size(); p++)
    {
        // (2) r * (c_0 + c_1 y + ... + c_D y^D): 실험 7 의 evaluate 와 같다.
        //     다른 점: 계수가 이미 평문으로 인코딩되어 있다 (coeffs[p][k]). 슬롯마다 다른 다항식.
        Ciphertext result;
        for (int k = 1; k <= D; k++)
        {
            Ciphertext term;
            evaluator.multiply_plain(powers[k], coeffs[p][k], term);
            if (k == 1)
            {
                result = term;
            }
            else
            {
                evaluator.add_inplace(result, term);
            }
        }
        evaluator.add_plain_inplace(result, coeffs[p][0]);
        evaluator.multiply_plain_inplace(result, pt_r[p]);

        // (3) 돌려보내기 전에 마지막 레벨로 mod switch: 암호문 크기가 RNS 소수 4 개 -> 1 개 (실험 2)
        evaluator.mod_switch_to_inplace(result, context.last_parms_id());

        responses[p] = result;
    }
    return responses;
}

int main()
{
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
    const size_t N = encoder.slot_count(); // 슬롯 수 = 해시 테이블 크기 8192
    const int H = 3;                       // 해시 함수 수

    // ---------------------------------------------------------------
    // 집합 만들기: |X| = 2^18, |Y| = 4000, 교집합 100
    // ---------------------------------------------------------------
    mt19937_64 rng(1);
    unordered_set<uint64_t> used;
    vector<uint64_t> X = random_items(size_t(1) << 18, t, rng, used);
    vector<uint64_t> Y(X.begin(), X.begin() + 100);
    vector<uint64_t> rest = random_items(4000 - 100, t, rng, used);
    Y.insert(Y.end(), rest.begin(), rest.end());
    unordered_set<uint64_t> X_set(X.begin(), X.end());

    // ---------------------------------------------------------------
    // 1. 해싱
    // ---------------------------------------------------------------
    vector<uint64_t> table;
    if (!cuckoo_insert(Y, N, H, rng, table))
    {
        cerr << "cuckoo hashing 실패\n";
        return 1;
    }
    vector<vector<uint64_t>> bucket = simple_hash(X, N, H);
    size_t B_max = max_bucket_size(bucket);

    // ---------------------------------------------------------------
    // 2. 보내는 쪽 준비 (받는 쪽과 상관없이 미리 해 둘 수 있다)
    //    bucket j 를 ALPHA 조각으로 나누고 (모자라면 PAD_ITEM 으로 채움), 조각마다 다항식 계수를 슬롯 j 에 넣는다
    // ---------------------------------------------------------------
    if (ALPHA < 1)
    {
        cerr << "ALPHA 는 1 이상이어야 합니다\n";
        return 1;
    }
    int D = static_cast<int>((B_max + ALPHA - 1) / ALPHA);
    Timer tm_setup;
    vector<vector<Plaintext>> coeffs(ALPHA, vector<Plaintext>(D + 1));
    for (int p = 0; p < ALPHA; p++)
    {
        vector<vector<uint64_t>> c(D + 1, vector<uint64_t>(N));
        for (size_t j = 0; j < N; j++)
        {
            vector<uint64_t> roots(D, PAD_ITEM);
            for (int k = 0; k < D; k++)
            {
                size_t idx = static_cast<size_t>(p) * D + k;
                if (idx < bucket[j].size())
                {
                    roots[k] = bucket[j][idx];
                }
            }
            vector<uint64_t> cj = poly_from_roots(roots, t);
            for (int k = 0; k <= D; k++)
            {
                c[k][j] = cj[k];
            }
        }
        for (int k = 0; k <= D; k++)
        {
            encoder.encode(c[k], coeffs[p][k]);
        }
    }
    vector<Plaintext> pt_r(ALPHA);
    for (int p = 0; p < ALPHA; p++)
    {
        vector<uint64_t> r(N);
        for (auto &v : r)
        {
            v = 1 + rng() % (t - 1);
        }
        encoder.encode(r, pt_r[p]);
    }
    double ms_setup = tm_setup.ms();

    cout << "\n|X| = " << X.size() << ", |Y| = " << Y.size() << ", |X n Y| = 100\n";
    cout << "largest bucket B_max = " << B_max << ",  ALPHA = " << ALPHA << "  ->  degree D = " << D << "\n";
    cout << "SEND = {";
    for (size_t i = 0; i < SEND.size(); i++)
    {
        cout << (i ? ", " : " ") << SEND[i];
    }
    cout << " }\n";
    cout << fixed << setprecision(1) << "sender setup (hashing excluded): " << ms_setup << " ms\n";
    cout.unsetf(ios::fixed);

    wait_enter("다음: 받는 쪽 -> 보내는 쪽 -> 받는 쪽");

    // ---------------------------------------------------------------
    // 3. 받는 쪽: 테이블의 y^k (k in SEND) 를 암호화해서 보낸다
    // ---------------------------------------------------------------
    vector<Ciphertext> powers(D + 1);
    double upload_mb = 0;
    for (int k : SEND)
    {
        if (k < 1 || k > D)
        {
            continue; // 필요 없는 거듭제곱
        }
        vector<uint64_t> yk(N);
        for (size_t j = 0; j < N; j++)
        {
            yk[j] = powmod(table[j], k, t);
        }
        Plaintext pt;
        encoder.encode(yk, pt);
        encryptor.encrypt(pt, powers[k]);
        upload_mb += powers[k].save_size(compr_mode_type::none) / 1e6;
    }

    // ---------------------------------------------------------------
    // 4. 보내는 쪽: 응답 계산
    // ---------------------------------------------------------------
    PowerPlan plan = plan_powers(SEND, D);
    Timer tm_server;
    vector<Ciphertext> responses = server_respond(powers, plan, coeffs, pt_r, evaluator, relin_keys, context);
    double ms_server = tm_server.ms();

    double download_mb = 0;
    int min_budget = 1000;
    for (size_t p = 0; p < responses.size(); p++)
    {
        require_ct("response " + to_string(p), responses[p]);
        download_mb += responses[p].save_size(compr_mode_type::none) / 1e6;
        min_budget = min(min_budget, decryptor.invariant_noise_budget(responses[p]));
    }

    // ---------------------------------------------------------------
    // 5. 받는 쪽: 응답 ALPHA 개 중 하나라도 0 인 슬롯 = 교집합
    // ---------------------------------------------------------------
    vector<bool> hit(N, false);
    for (auto &ct : responses)
    {
        Plaintext pt;
        decryptor.decrypt(ct, pt);
        vector<uint64_t> v;
        encoder.decode(pt, v);
        for (size_t j = 0; j < N; j++)
        {
            if (v[j] == 0)
            {
                hit[j] = true;
            }
        }
    }
    int found = 0, wrong = 0;
    for (size_t j = 0; j < N; j++)
    {
        bool truth = table[j] != EMPTY_SLOT && X_set.count(table[j]) > 0;
        if (hit[j] && truth)
        {
            found++;
        }
        else if (hit[j] || truth)
        {
            wrong++;
        }
    }

    cout << fixed << setprecision(2);
    cout << "\nupload   (receiver -> sender): " << setw(3) << plan.split.size() - 1 - plan.mult_count
         << " ciphertexts " << setw(8) << upload_mb << " MB\n";
    cout << "download (sender -> receiver): " << setw(3) << responses.size() << " ciphertexts " << setw(8)
         << download_mb << " MB\n";
    cout << setprecision(1) << "server: " << plan.mult_count << " ct-ct mults, depth " << plan.max_depth << ",  "
         << ms_server << " ms\n";
    cout.unsetf(ios::fixed);
    cout << "min noise budget of responses: " << min_budget << " bits  (실습용 확인: 실제 서버는 비밀키가 없다)\n";
    cout << "found " << found << " / 100,  wrong " << wrong << (found == 100 && wrong == 0 ? "  -> OK" : "  -> FAIL")
         << "\n";

    return 0;
}
