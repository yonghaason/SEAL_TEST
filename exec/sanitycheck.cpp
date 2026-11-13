// 빌드 환경 확인용: CKKS 로 x * y + x 를 계산한다. "x * y + x = 1.5 3 4.5 6" 이 나오면 정상.
//   빌드: cmake --build build --target sanitycheck
//   실행: ./build/sanitycheck
#include <cmath>
#include <iostream>
#include <vector>
#include "seal/seal.h"

using namespace std;
using namespace seal;

int main()
{
    // CKKS 파라미터
    EncryptionParameters parms(scheme_type::ckks);
    size_t poly_modulus_degree = 8192;
    parms.set_poly_modulus_degree(poly_modulus_degree);
    parms.set_coeff_modulus(CoeffModulus::Create(poly_modulus_degree, { 60, 40, 40, 60 }));
    double scale = pow(2.0, 40);

    SEALContext context(parms);

    // 키 생성
    KeyGenerator keygen(context);
    SecretKey secret_key = keygen.secret_key();
    PublicKey public_key;
    keygen.create_public_key(public_key);
    RelinKeys relin_keys;
    keygen.create_relin_keys(relin_keys);

    Encryptor encryptor(context, public_key);
    Evaluator evaluator(context);
    Decryptor decryptor(context, secret_key);
    CKKSEncoder encoder(context);

    // x = (1, 2, 3, 4), y = (0.5, 0.5, 0.5, 0.5) 를 인코딩, 암호화
    vector<double> x = { 1.0, 2.0, 3.0, 4.0 };
    vector<double> y = { 0.5, 0.5, 0.5, 0.5 };
    Plaintext x_plain, y_plain;
    encoder.encode(x, scale, x_plain);
    encoder.encode(y, scale, y_plain);
    Ciphertext x_enc, y_enc;
    encryptor.encrypt(x_plain, x_enc);
    encryptor.encrypt(y_plain, y_enc);

    // 암호문 상태에서 x * y + x 계산
    Ciphertext prod;
    evaluator.multiply(x_enc, y_enc, prod);
    evaluator.relinearize_inplace(prod, relin_keys);
    evaluator.rescale_to_next_inplace(prod);
    prod.scale() = scale;
    evaluator.mod_switch_to_inplace(x_enc, prod.parms_id());
    Ciphertext result;
    evaluator.add(prod, x_enc, result);

    // 복호화, 디코딩
    Plaintext result_plain;
    decryptor.decrypt(result, result_plain);
    vector<double> out;
    encoder.decode(result_plain, out);

    cout << "x * y + x = ";
    for (size_t i = 0; i < x.size(); i++)
    {
        cout << out[i] << " ";
    }
    cout << endl;

    return 0;
}
