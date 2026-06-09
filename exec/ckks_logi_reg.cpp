#include "seal/seal.h"
#include "mnist_model.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;
using namespace seal;

namespace {

using Clock = chrono::steady_clock;

double elapsed_ms(Clock::time_point start, Clock::time_point end) {
    return chrono::duration<double, milli>(end - start).count();
}

vector<Ciphertext> server_compute_scores(
    const Ciphertext& x_encrypted,
    const vector<vector<double>>& weights,
    const vector<double>& bias,
    CKKSEncoder& encoder,
    Evaluator& evaluator,
    const GaloisKeys& galois_keys,
    double scale
) 
{
    
}

vector<double> client_decrypt_scores(
    const vector<Ciphertext>& encrypted_scores,
    CKKSEncoder& encoder,
    Decryptor& decryptor
) 
{
    
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <vectorized_image>\n";
        cerr << "Example: python python_helper/image_converter.py <your image file> -o vectorized_image.txt\n";
        cerr << "         " << argv[0] << " vectorized_image.txt\n";
        return 1;
    }

    const size_t poly_modulus_degree = 8192;
    const double scale = pow(2.0, 40);

    EncryptionParameters parms(scheme_type::ckks);
    parms.set_poly_modulus_degree(poly_modulus_degree);
    parms.set_coeff_modulus(CoeffModulus::Create(
        poly_modulus_degree,
        {60, 40, 40, 60}
    ));

    SEALContext context(parms);
    KeyGenerator keygen(context);

    SecretKey secret_key = keygen.secret_key();
    PublicKey public_key;
    keygen.create_public_key(public_key);

    GaloisKeys galois_keys;
    keygen.create_galois_keys(galois_keys);

    Encryptor encryptor(context, public_key);
    Evaluator evaluator(context);
    Decryptor decryptor(context, secret_key);
    CKKSEncoder encoder(context);

    // 1. Client는 "image vector"를 암호화한다.
    auto step1_start = Clock::now();

    vector<double> x = load_input_vector(argv[1]);
    vector<double> x_slots(encoder.slot_count(), 0.0);
    copy(x.begin(), x.end(), x_slots.begin());

    Plaintext x_plain;
    encoder.encode(x_slots, scale, x_plain);

    Ciphertext x_encrypted;
    encryptor.encrypt(x_plain, x_encrypted);
    auto step1_end = Clock::now();

    // 2. Server는 암호화된 상태로 logistic regression의 Inference를 수행한다
    // 즉, Score 계산 = 내적 연산을 수행한다.
    auto step2_start = Clock::now();

    vector<vector<double>> weights = mnist_weights();
    vector<double> bias = mnist_bias();
    vector<Ciphertext> encrypted_scores = server_compute_scores(
        x_encrypted, weights, bias, encoder, evaluator, galois_keys, scale);
    auto step2_end = Clock::now();

    // 3. Client는 server가 돌려준 score ciphertext들을 복호화하고,
    //     가장 큰 score를 가진 숫자를 최종 예측값으로 선택한다.
    auto step3_start = Clock::now();
    vector<double> ckks_scores =
        client_decrypt_scores(encrypted_scores, encoder, decryptor);
    auto step3_end = Clock::now();

    auto plain_check_start = Clock::now();
    vector<double> expected_scores = plain_scores(x, weights, bias);
    double max_abs_error = 0.0;
    for (size_t k = 0; k < ckks_scores.size(); ++k) {
        max_abs_error = max(max_abs_error, abs(ckks_scores[k] - expected_scores[k]));
    }
    auto plain_check_end = Clock::now();

    cout << fixed << setprecision(6);
    cout << "Timing:\n";
    cout << "Step 1 [Client encrypt image]: "
         << elapsed_ms(step1_start, step1_end) << " ms\n";
    cout << "Step 2 [Server encrypted inference]: "
         << elapsed_ms(step2_start, step2_end) << " ms\n";
    cout << "Step 3* [Client decrypt scores]: "
         << elapsed_ms(step3_start, step3_end) << " ms\n";
    cout << "Plain check: "
         << elapsed_ms(plain_check_start, plain_check_end) << " ms\n";
    cout << "* Step 3 note: only the client uses the secret key; the server sees ciphertext scores only.\n\n";

    cout << "Scores:\n";
    for (size_t k = 0; k < ckks_scores.size(); ++k) {
        cout << k << ": " << ckks_scores[k] << '\n';
    }

    cout << "\nPrediction: " << max_element(ckks_scores.begin(), ckks_scores.end()) - ckks_scores.begin() << '\n';
    cout << "Plain check max abs error: " << scientific << setprecision(3) << max_abs_error << '\n';
}
