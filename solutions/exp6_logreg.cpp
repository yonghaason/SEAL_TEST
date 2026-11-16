//   빌드: cmake --build build --target exp6_logreg
//   실행: ./build/exp6_logreg            (내장 이미지 0 번)
//         ./build/exp6_logreg 7          (내장 이미지 0 ~ 10 번 중 하나)
//         ./build/exp6_logreg my.txt     (tools/image_to_vector.py 로 만든 784 개 값 파일)
#include <cmath>
#include <fstream>
#include "common.h"
#include "mnist_data.h"

using namespace std;
using namespace seal;

// 이미지 한 장이 차지하는 슬롯 수 (784 를 2 의 거듭제곱으로 올림). 남는 슬롯은 0 으로 둔다.
const size_t BLOCK = 1024;

// 입력 이미지 (길이 784, 값 0 ~ 1) 를 정한다. 인자가 없으면 내장 이미지 0 번.
vector<double> load_image(int argc, char **argv, string &name)
{
    string arg = (argc > 1) ? argv[1] : "0";
    vector<double> x;
    if (arg.find_first_not_of("0123456789") == string::npos && stoi(arg) < MNIST_IMAGES)
    {
        int idx = stoi(arg);
        for (int j = 0; j < MNIST_DIM; j++)
        {
            x.push_back(mnist_pixel[idx][j] / 255.0);
        }
        name = "built-in image " + arg + " (label " + to_string(mnist_label[idx]) + ")";
        return x;
    }
    ifstream in(arg);
    double v;
    while (in >> v)
    {
        x.push_back(v);
    }
    if (x.size() != static_cast<size_t>(MNIST_DIM))
    {
        cerr << "[오류] '" << arg << "' 에서 값 784 개를 읽지 못했습니다 (읽은 개수: " << x.size() << ").\n"
             << "       내장 이미지는 0 ~ " << MNIST_IMAGES - 1 << " 번입니다.\n";
        exit(1);
    }
    name = "file " + arg;
    return x;
}

// 28 x 28 이미지를 글자로 출력한다.
void print_image(const vector<double> &x)
{
    for (int r = 0; r < 28; r++)
    {
        for (int c = 0; c < 28; c++)
        {
            double p = x[r * 28 + c];
            cout << (p > 0.63 ? "##" : p > 0.24 ? "++" : "  ");
        }
        cout << "\n";
    }
}

// 암호문 ct_x 와 평문 벡터 w_slots 를 슬롯별로 곱하고, 1024 칸 블록마다 합을 구한 뒤 (회전-합), b_slots 를 더한다.
// 결과 암호문의 슬롯 1024 j 에  (블록 j 의 내적) + b_slots[1024 j]  가 들어간다. 나머지 슬롯은 쓰지 않는다.
Ciphertext block_inner_product(const Ciphertext &ct_x, const vector<double> &w_slots, const vector<double> &b_slots,
                               Evaluator &evaluator, CKKSEncoder &encoder, const GaloisKeys &galois_keys,
                               double scale)
{
    // (1) 슬롯별 곱: w_slots 를 인코딩해서 ct_x 에 곱하고 rescale
    Plaintext pt_w;
    encoder.encode(w_slots, scale, pt_w);
    Ciphertext ct_z;
    evaluator.multiply_plain(ct_x, pt_w, ct_z);
    evaluator.rescale_to_next_inplace(ct_z);

    // (2) 회전-합: step = 512, 256, ..., 1 에 대해 ct_z 를 step 칸 회전한 것을 ct_z 에 더한다
    //     10 번 하면 슬롯 j 에 원래 슬롯 j ~ j + 1023 의 합이 들어간다
    for (int step = BLOCK / 2; step >= 1; step /= 2)
    {
        Ciphertext ct_rot;
        evaluator.rotate_vector(ct_z, step, galois_keys, ct_rot);
        evaluator.add_inplace(ct_z, ct_rot);
    }

    // (3) b_slots 더하기: ct_z 와 같은 레벨, 같은 scale 로 인코딩해서 add_plain
    Plaintext pt_b;
    encoder.encode(b_slots, ct_z.parms_id(), ct_z.scale(), pt_b);
    evaluator.add_plain_inplace(ct_z, pt_b);

    return ct_z;
}

// 암호문을 복호화해서 모든 슬롯 값을 돌려준다.
vector<double> decrypt_slots(const string &label, const Ciphertext &ct, Decryptor &decryptor, CKKSEncoder &encoder)
{
    require_ct(label, ct);
    Plaintext pt;
    decryptor.decrypt(ct, pt);
    vector<double> slots;
    encoder.decode(pt, slots);
    return slots;
}

// 가장 큰 값의 위치
int argmax(const vector<double> &v)
{
    return static_cast<int>(max_element(v.begin(), v.end()) - v.begin());
}

int main(int argc, char **argv)
{
    string image_name;
    vector<double> x = load_image(argc, argv, image_name);

    // 곱셈 깊이 1 (가중치 곱 한 번) -> Delta 크기 소수 1 개
    // 실험 5 와 같은 n = 8192: 슬롯 n / 2 = 4096 개 = 1024 칸 블록 4 개
    double scale = pow(2.0, 40);

    EncryptionParameters parms(scheme_type::ckks);
    size_t n = 8192;
    parms.set_poly_modulus_degree(n);
    parms.set_coeff_modulus(CoeffModulus::Create(n, { 60, 40, 60 }));

    SEALContext context(parms);
    print_params(context);

    KeyGenerator keygen(context);
    SecretKey secret_key = keygen.secret_key();
    PublicKey public_key;
    keygen.create_public_key(public_key);

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
    size_t slots = encoder.slot_count();
    const int BLOCKS = static_cast<int>(slots / BLOCK);              // 암호문 하나에 들어가는 블록 수 (4)
    const int GROUPS = (MNIST_CLASSES + BLOCKS - 1) / BLOCKS;        // (b) 에서 행을 BLOCKS 개씩 묶은 묶음 수 (3)

    // 비교용 평문 계산: 점수 z = W x + b (클래스마다 내적 하나)
    vector<double> z_plain(MNIST_CLASSES);
    for (int k = 0; k < MNIST_CLASSES; k++)
    {
        z_plain[k] = mnist_b[k];
        for (int j = 0; j < MNIST_DIM; j++)
        {
            z_plain[k] += mnist_w[k][j] * x[j];
        }
    }

    cout << "\ninput: " << image_name << "\n";
    print_image(x);
    wait_enter("다음: 클라이언트가 이미지를 암호화");

    // ---------------------------------------------------------------
    // 1. 클라이언트: 이미지를 블록 4 개에 똑같이 복사해서 암호문 하나로 암호화
    //    x_slots = [ x (784) 0 ... 0 | x (784) 0 ... 0 | x (784) 0 ... 0 | x (784) 0 ... 0 ]
    // ---------------------------------------------------------------
    vector<double> x_slots(slots, 0.0);
    for (int t = 0; t < BLOCKS; t++)
    {
        for (int j = 0; j < MNIST_DIM; j++)
        {
            x_slots[BLOCK * t + j] = x[j];
        }
    }
    Plaintext pt_x;
    encoder.encode(x_slots, scale, pt_x);
    Ciphertext ct_x;
    encryptor.encrypt(pt_x, ct_x);
    print_ct_ckks("fresh", ct_x, context);

    wait_enter("다음: 서버가 (a) 한 행씩 계산");

    // ---------------------------------------------------------------
    // 2. 서버 (a): 한 행씩. 클래스 k 마다 W 의 k 번째 행을 블록 0 에만 넣고 내적 -> 암호문 10 개
    //    w_slots = [ W[k] (784) 0 ... 0 | 0 ... ],  결과는 슬롯 0
    // ---------------------------------------------------------------
    Timer tm_a;
    vector<Ciphertext> ct_rows(MNIST_CLASSES);
    for (int k = 0; k < MNIST_CLASSES; k++)
    {
        vector<double> w_slots(slots, 0.0), b_slots(slots, 0.0);
        for (int j = 0; j < MNIST_DIM; j++)
        {
            w_slots[j] = mnist_w[k][j];
        }
        b_slots[0] = mnist_b[k];
        ct_rows[k] = block_inner_product(ct_x, w_slots, b_slots, evaluator, encoder, galois_keys, scale);
    }
    double ms_a = tm_a.ms();
    print_ct_ckks("(a) row 0", ct_rows[0], context);

    wait_enter("다음: 서버가 (b) 행을 4 개씩 블록에 나눠 담아 계산");

    // ---------------------------------------------------------------
    // 3. 서버 (b): 행을 4 개씩 묶어 블록 4 개에 나눠 담는다. 묶음마다 내적 한 번 -> 암호문 3 개
    //    묶음 c 의 w_slots = [ W[4c] (784) 0 ... 0 | W[4c+1] ... | W[4c+2] ... | W[4c+3] ... ]  (마지막 묶음은 W[8], W[9] 만)
    //    b_slots: 슬롯 1024 t 에 b[4c + t],  결과는 묶음 c 의 슬롯 1024 t 에 z[4c + t]
    // ---------------------------------------------------------------
    Timer tm_b;
    vector<Ciphertext> ct_packed(GROUPS);
    for (int c = 0; c < GROUPS; c++)
    {
        vector<double> w_slots(slots, 0.0), b_slots(slots, 0.0);
        for (int t = 0; t < BLOCKS && BLOCKS * c + t < MNIST_CLASSES; t++)
        {
            int k = BLOCKS * c + t;
            for (int j = 0; j < MNIST_DIM; j++)
            {
                w_slots[BLOCK * t + j] = mnist_w[k][j];
            }
            b_slots[BLOCK * t] = mnist_b[k];
        }
        require_filled("(b) w_slots", w_slots);
        ct_packed[c] = block_inner_product(ct_x, w_slots, b_slots, evaluator, encoder, galois_keys, scale);
    }
    double ms_b = tm_b.ms();
    print_ct_ckks("(b) group 0", ct_packed[0], context);

    wait_enter("다음: 클라이언트가 복호화");

    // ---------------------------------------------------------------
    // 4. 클라이언트: 복호화해서 점수가 가장 큰 클래스를 고른다
    // ---------------------------------------------------------------
    vector<double> z_a(MNIST_CLASSES), z_b(MNIST_CLASSES);
    vector<vector<double>> packed(GROUPS);
    for (int c = 0; c < GROUPS; c++)
    {
        packed[c] = decrypt_slots("(b) group " + to_string(c), ct_packed[c], decryptor, encoder);
    }
    for (int k = 0; k < MNIST_CLASSES; k++)
    {
        z_a[k] = decrypt_slots("(a) row " + to_string(k), ct_rows[k], decryptor, encoder)[0];
        z_b[k] = packed[k / BLOCKS][BLOCK * (k % BLOCKS)];   // 클래스 k 는 묶음 k / 4 의 블록 k % 4
    }

    cout << "\n" << setw(6) << "class" << setw(12) << "plain" << setw(12) << "(a) rows" << setw(12) << "(b) packed"
         << "\n";
    double e_a = 0, e_b = 0;
    cout << fixed << setprecision(4);
    for (int k = 0; k < MNIST_CLASSES; k++)
    {
        e_a = max(e_a, abs(z_a[k] - z_plain[k]));
        e_b = max(e_b, abs(z_b[k] - z_plain[k]));
        cout << setw(6) << k << setw(12) << z_plain[k] << setw(12) << z_a[k] << setw(12) << z_b[k] << "\n";
    }
    cout.unsetf(ios::fixed);
    cout << setw(6) << "pred" << setw(12) << argmax(z_plain) << setw(12) << argmax(z_a) << setw(12) << argmax(z_b)
         << "\n";

    // 이론값: 회전 (key switching) 횟수 = 내적 횟수 x 10.  시간은 회전 횟수에 거의 비례한다 (실험 4)
    //  - (a) 내적 10 번 -> 회전 100 번, 서버가 돌려주는 암호문 10 개
    //  - (b) 내적  3 번 -> 회전  30 번, 서버가 돌려주는 암호문  3 개 (행 4 개씩 묶음 3 개)
    cout << "\n" << left << setw(16) << "strategy" << right << setw(14) << "ciphertexts" << setw(12) << "rotations"
         << setw(14) << "time" << setw(16) << "CKKS error" << "\n";
    auto row = [](const string &name, int cts, int rotations, double ms, double err) {
        cout << left << setw(16) << name << right << setw(14) << cts << setw(12) << rotations << fixed
             << setprecision(1) << setw(11) << ms << " ms" << scientific << setprecision(2) << setw(16) << err << "\n";
        cout.unsetf(ios::floatfield);
    };
    row("(a) row by row", MNIST_CLASSES, MNIST_CLASSES * 10, ms_a, e_a);
    row("(b) packed", GROUPS, GROUPS * 10, ms_b, e_b);

    return 0;
}
