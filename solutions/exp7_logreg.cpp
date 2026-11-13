//   빌드: cmake --build build --target exp7_logreg
//   실행: ./build/exp7_logreg
#include <cmath>
#include "common.h"
#include "mnist_data.h"

using namespace std;
using namespace seal;

// 이미지 한 장이 차지하는 슬롯 수 (784 를 2 의 거듭제곱으로 올림). 남는 슬롯은 0 으로 둔다.
const size_t BLOCK = 1024;

double sigmoid(double z)
{
    return 1.0 / (1.0 + exp(-z));
}
double sigmoid3_plain(double z)
{
    return 0.5 + 0.197 * z - 0.004 * z * z * z;
}

// 이미지 first 번과 first + 1 번을 나란히 출력한다 (28 x 28 중 숫자가 있는 가운데 부분만).
void print_images(int first)
{
    for (int r = 4; r < 24; r++)
    {
        for (int idx = first; idx < first + 2; idx++)
        {
            for (int c = 4; c < 23; c++)
            {
                int p = mnist_pixel[idx][r * 28 + c];
                cout << (p > 160 ? "##" : p > 60 ? "++" : "  ");
            }
            cout << "  ";
        }
        cout << "\n";
    }
}

// 암호문 ct_z 의 슬롯마다 0.5 + 0.197 z - 0.004 z^3 을 계산해서 돌려준다 (실험 6 과 같은 함수).
Ciphertext sigmoid3(const Ciphertext &ct_z, Evaluator &evaluator, CKKSEncoder &encoder,
                    const RelinKeys &relin_keys, double scale)
{
    // z^2: 곱셈 -> relinearize -> rescale
    Ciphertext ct_z2;
    evaluator.multiply(ct_z, ct_z, ct_z2);
    evaluator.relinearize_inplace(ct_z2, relin_keys);
    evaluator.rescale_to_next_inplace(ct_z2);

    // -0.004 z: 상수를 곱하고 rescale
    Plaintext pt_c3;
    encoder.encode(-0.004, ct_z.parms_id(), scale, pt_c3);
    Ciphertext ct_c3z;
    evaluator.multiply_plain(ct_z, pt_c3, ct_c3z);
    evaluator.rescale_to_next_inplace(ct_c3z);

    // -0.004 z^3 = z^2 * (-0.004 z): 곱셈 -> relinearize -> rescale
    Ciphertext ct_c3z3;
    evaluator.multiply(ct_z2, ct_c3z, ct_c3z3);
    evaluator.relinearize_inplace(ct_c3z3, relin_keys);
    evaluator.rescale_to_next_inplace(ct_c3z3);

    // 0.197 z: 상수를 곱하고 rescale, 그리고 ct_c3z3 와 같은 레벨로 내리기
    Plaintext pt_c1;
    encoder.encode(0.197, ct_z.parms_id(), scale, pt_c1);
    Ciphertext ct_c1z;
    evaluator.multiply_plain(ct_z, pt_c1, ct_c1z);
    evaluator.rescale_to_next_inplace(ct_c1z);
    evaluator.mod_switch_to_inplace(ct_c1z, ct_c3z3.parms_id());

    // scale 을 Delta 로 맞추고 더하기. 상수 0.5 는 같은 레벨로 인코딩해서 add_plain
    ct_c3z3.scale() = scale;
    ct_c1z.scale() = scale;
    Ciphertext ct_out;
    evaluator.add(ct_c3z3, ct_c1z, ct_out);
    Plaintext pt_c0;
    encoder.encode(0.5, ct_out.parms_id(), scale, pt_c0);
    evaluator.add_plain_inplace(ct_out, pt_c0);

    return ct_out;
}

// 암호문을 복호화해서 이미지별 결과 (슬롯 0, BLOCK, 2 BLOCK, ...) 만 꺼낸다.
vector<double> decrypt_results(const string &label, const Ciphertext &ct, Decryptor &decryptor,
                               CKKSEncoder &encoder)
{
    require_ct(label, ct);
    Plaintext pt;
    decryptor.decrypt(ct, pt);
    vector<double> slots;
    encoder.decode(pt, slots);
    vector<double> out(MNIST_IMAGES);
    for (int i = 0; i < MNIST_IMAGES; i++)
    {
        out[i] = slots[BLOCK * i];
    }
    return out;
}

int main()
{
    // 곱셈 깊이 3 (가중치 곱 1 + 시그모이드 근사 2) -> Delta 크기 소수 3 개
    // log Pq = 240 이라 n = 8192 (최대 218) 로는 안 되고 n = 16384 가 필요하다 (실험 0)
    double scale = pow(2.0, 40);

    EncryptionParameters parms(scheme_type::ckks);
    size_t n = 16384;
    parms.set_poly_modulus_degree(n);
    parms.set_coeff_modulus(CoeffModulus::Create(n, { 60, 40, 40, 40, 60 }));

    SEALContext context(parms);
    print_params(context);

    KeyGenerator keygen(context);
    SecretKey secret_key = keygen.secret_key();
    PublicKey public_key;
    keygen.create_public_key(public_key);
    RelinKeys relin_keys;
    keygen.create_relin_keys(relin_keys);

    // 회전 키는 필요한 칸 수 (512, 256, ..., 2, 1) 만 만든다 (실험 4)
    vector<int> steps;
    for (int step = BLOCK / 2; step >= 1; step /= 2)
    {
        steps.push_back(step);
    }
    GaloisKeys galois_keys;
    keygen.create_galois_keys(steps, galois_keys);

    Encryptor encryptor(context, public_key);
    Decryptor decryptor(context, secret_key);
    Evaluator evaluator(context);
    CKKSEncoder encoder(context);

    // 슬롯 n / 2 = 8192 개에 이미지 8 장을 1024 칸씩 나눠 담는다
    //   x_slots = [ 이미지 0 (784) 0 ... 0 | 이미지 1 (784) 0 ... 0 | ... ]
    //   w_slots = [ 가중치 w (784) 0 ... 0 | 가중치 w (784) 0 ... 0 | ... ]
    size_t slots = encoder.slot_count();
    vector<double> x_slots(slots, 0.0), w_slots(slots, 0.0);
    // 비교용 평문 계산: 로짓 z = <w, x> + b, 시그모이드, 3차 근사
    vector<double> z_plain(MNIST_IMAGES), prob_exact(MNIST_IMAGES), prob_approx(MNIST_IMAGES);
    for (int i = 0; i < MNIST_IMAGES; i++)
    {
        double z = mnist_b;
        for (int j = 0; j < MNIST_DIM; j++)
        {
            x_slots[BLOCK * i + j] = mnist_pixel[i][j] / 255.0;
            w_slots[BLOCK * i + j] = mnist_w[j];
            z += mnist_w[j] * x_slots[BLOCK * i + j];
        }
        z_plain[i] = z;
        prob_exact[i] = sigmoid(z);
        prob_approx[i] = sigmoid3_plain(z);
    }

    cout << "\nimage 0 (label " << mnist_label[0] << ") and image 1 (label " << mnist_label[1] << ")\n";
    print_images(0);
    wait_enter("다음: 클라이언트가 이미지를 암호화");

    // ---------------------------------------------------------------
    // 1. 클라이언트: 이미지 8 장을 암호문 하나로 암호화
    // ---------------------------------------------------------------
    Plaintext pt_x;
    encoder.encode(x_slots, scale, pt_x);
    Ciphertext ct_x;
    encryptor.encrypt(pt_x, ct_x);
    print_ct_ckks("fresh (8 images)", ct_x, context);

    wait_enter("다음: 서버가 암호문 상태로 로짓 계산");

    // ---------------------------------------------------------------
    // 2. 서버: 로짓 z = <w, x> + b  (서버는 secret key 가 없다)
    // ---------------------------------------------------------------
    Timer tm_logit;

    // (a) 슬롯별 곱: 가중치 w_slots 를 인코딩해서 ct_x 에 곱하고 rescale -> ct_z
    Plaintext pt_w;
    encoder.encode(w_slots, scale, pt_w);
    Ciphertext ct_z;
    evaluator.multiply_plain(ct_x, pt_w, ct_z);
    evaluator.rescale_to_next_inplace(ct_z);

    // (b) 회전-합: step = 512, 256, ..., 1 에 대해 ct_z 를 step 칸 회전한 것을 ct_z 에 더한다
    //     10 번 하면 슬롯 j 에 원래 슬롯 j ~ j + 1023 의 합이 들어간다 -> 슬롯 1024 i = 이미지 i 의 내적
    for (int step = BLOCK / 2; step >= 1; step /= 2)
    {
        Ciphertext ct_rot;
        evaluator.rotate_vector(ct_z, step, galois_keys, ct_rot);
        evaluator.add_inplace(ct_z, ct_rot);
    }

    // (c) 상수 b 더하기: ct_z 와 같은 레벨, 같은 scale 로 인코딩해서 add_plain
    Plaintext pt_b;
    encoder.encode(mnist_b, ct_z.parms_id(), ct_z.scale(), pt_b);
    evaluator.add_plain_inplace(ct_z, pt_b);

    double ms_logit = tm_logit.ms();
    print_ct_ckks("logit z", ct_z, context);

    wait_enter("다음: 서버가 시그모이드 근사 계산");

    // ---------------------------------------------------------------
    // 3. 서버: 확률 = sigmoid3(z)
    // ---------------------------------------------------------------
    Timer tm_sigmoid;
    Ciphertext ct_prob = sigmoid3(ct_z, evaluator, encoder, relin_keys, scale);
    double ms_sigmoid = tm_sigmoid.ms();
    print_ct_ckks("sigmoid3(z)", ct_prob, context);

    wait_enter("다음: 클라이언트가 복호화");

    // ---------------------------------------------------------------
    // 4. 클라이언트: 복호화해서 확률이 0.5 보다 크면 1, 아니면 0 으로 판정
    // ---------------------------------------------------------------
    vector<double> z_ckks = decrypt_results("logit z", ct_z, decryptor, encoder);
    vector<double> prob_ckks = decrypt_results("sigmoid3(z)", ct_prob, decryptor, encoder);

    cout << "\n" << setw(5) << "image" << setw(7) << "label" << setw(10) << "z plain" << setw(10) << "z CKKS"
         << setw(10) << "sigmoid" << setw(10) << "poly" << setw(10) << "CKKS" << setw(6) << "pred" << "\n";
    double e_logit = 0, e_ckks = 0, e_approx = 0;
    int correct = 0;
    cout << fixed << setprecision(4);
    for (int i = 0; i < MNIST_IMAGES; i++)
    {
        int pred = prob_ckks[i] > 0.5 ? 1 : 0;
        correct += (pred == mnist_label[i]);
        e_logit = max(e_logit, abs(z_ckks[i] - z_plain[i]));
        e_ckks = max(e_ckks, abs(prob_ckks[i] - prob_approx[i]));
        e_approx = max(e_approx, abs(prob_approx[i] - prob_exact[i]));
        cout << setw(5) << i << setw(7) << mnist_label[i] << setw(10) << z_plain[i] << setw(10) << z_ckks[i]
             << setw(10) << prob_exact[i] << setw(10) << prob_approx[i] << setw(10) << prob_ckks[i] << setw(6)
             << pred << (pred == mnist_label[i] ? "  OK" : "  FAIL") << "\n";
    }
    cout.unsetf(ios::fixed);

    //  - CKKS 오차: |복호화 결과 - 평문 계산|  (로짓, 확률 각각)
    //  - 근사 오차: |다항식 - 시그모이드|
    //  - key switching: 회전 10 번 + relinearize 2 번
    cout << "\naccuracy: " << correct << " / " << MNIST_IMAGES << "\n";
    cout << scientific << setprecision(2);
    cout << "CKKS error (logit) = " << e_logit << "\n";
    cout << "CKKS error (prob)  = " << e_ckks << "\n";
    cout << "approx error       = " << e_approx << "\n";
    cout << fixed << setprecision(1);
    cout << "time: logit (10 rotations) = " << ms_logit << " ms,  sigmoid3 (2 relinearizations) = " << ms_sigmoid
         << " ms\n";
    cout.unsetf(ios::floatfield);

    return 0;
}
