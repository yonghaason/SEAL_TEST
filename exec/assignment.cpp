#include "seal/seal.h"

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;
using namespace seal;

namespace {

constexpr size_t kPolyModulusDegree = 8192;
constexpr int kPlainModulusBits = 20;

EncryptionParameters make_bfv_parameters() {
    EncryptionParameters parms(scheme_type::bfv);
    parms.set_poly_modulus_degree(kPolyModulusDegree);
    parms.set_coeff_modulus(CoeffModulus::BFVDefault(kPolyModulusDegree));
    parms.set_plain_modulus(
        PlainModulus::Batching(kPolyModulusDegree, kPlainModulusBits));
    return parms;
}

vector<uint64_t> sample_random(size_t length, uint64_t plain_modulus) {
    random_device rd;
    mt19937_64 gen(rd());
    uniform_int_distribution<uint64_t> dist(1, plain_modulus - 1);

    vector<uint64_t> r(length, 1);
    for (size_t i = 0; i < length; ++i) {
        r[i] = dist(gen);
    }
    return r;
}

} 

// Server가 데이터 y_i를 가지고 있을 때,
// x_i를 각 Slot에 가지고 있는 암호문 "input_ctxt_filename"을 받아,
// r_i * (x_i - y_i)를 각 Slot에 가지고 있는 암호문을 계산하여
// "out_ctxt_filename"에 저장.

void server_function(
    const string& input_ctxt_filename, 
    const vector<uint64_t>& y, 
    const string& out_ctxt_filename) 
{
    EncryptionParameters parms = make_bfv_parameters();
    SEALContext context(parms);
    const uint64_t plain_modulus =
        context.first_context_data()->parms().plain_modulus().value();

}

// Server가 계산해준 암호문 "input_ctxt_filename",
// 및 본인이 가지고 있던 "secret_key_filename"을 로드하여
// 복호화를 수행하고, 같은 원소를 가지고 있는 slot index를 return
vector<size_t> client_function(
    const string& input_ctxt_filename,
    const string& secret_key_filename) {
    EncryptionParameters parms = make_bfv_parameters();
    SEALContext context(parms);

}

int test() {
    const string input_ctxt_filename = "equality_test_input.ct";
    const string secret_key_filename = "equality_test_secret.key";
    const string output_ctxt_filename = "equality_test_output.ct";
    
    const vector<size_t> expected_slots = {3, 7, 10, 17, 25};
    struct CleanupTestFiles {
        string input_ctxt_filename;
        string secret_key_filename;
        string output_ctxt_filename;

        ~CleanupTestFiles() {
            remove(input_ctxt_filename.c_str());
            remove(secret_key_filename.c_str());
            remove(output_ctxt_filename.c_str());
        }
    } cleanup{input_ctxt_filename, secret_key_filename, output_ctxt_filename};

    EncryptionParameters parms = make_bfv_parameters();
    SEALContext context(parms);
    KeyGenerator keygen(context);
    const uint64_t plain_modulus =
        context.first_context_data()->parms().plain_modulus().value();

    SecretKey secret_key = keygen.secret_key();
    PublicKey public_key;
    keygen.create_public_key(public_key);

    BatchEncoder encoder(context);
    Encryptor encryptor(context, public_key);

    vector<uint64_t> x(encoder.slot_count(), 0);
    vector<uint64_t> y(encoder.slot_count(), 0);
    for (size_t i = 0; i < encoder.slot_count(); ++i) {
        x[i] = (17 * i + 5) % (plain_modulus - 2);
        y[i] = x[i] + 1;
    }
    for (size_t slot : expected_slots) {
        y[slot] = x[slot];
    }
    
    Plaintext x_plain;
    encoder.encode(x, x_plain);

    Ciphertext query_ciphertext;
    encryptor.encrypt(x_plain, query_ciphertext);

    ofstream ct_out(input_ctxt_filename);
    query_ciphertext.save(ct_out);
    ct_out.close();

    ofstream sk_out(secret_key_filename);
    secret_key.save(sk_out);
    sk_out.close();

    server_function(input_ctxt_filename, y, output_ctxt_filename);
    vector<size_t> zero_slots =
        client_function(output_ctxt_filename, secret_key_filename);

    if (zero_slots != expected_slots) {
        throw runtime_error("test failed: expected only slots 3 and 7 to decrypt to 0");
    }

    cout << "test passed\n";

    return 0;
}

int main() {
    return test();
}
