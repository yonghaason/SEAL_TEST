#include "seal/seal.h"
#include "utils.h"

using namespace std;
using namespace seal;

int main(int argc, char** argv) {
  size_t poly_modulus_degree = 8192;
  EncryptionParameters parms(scheme_type::bfv);
  parms.set_poly_modulus_degree(poly_modulus_degree);
  // q 를 설정하는 파트
  parms.set_coeff_modulus(CoeffModulus::BFVDefault(poly_modulus_degree)); // 214 비트.
  // p 를 설정하는 파트: 20비트 소수를 설정하겠다.
  parms.set_plain_modulus(
      PlainModulus::Batching(poly_modulus_degree, 20));

  // 연산을 빠르게 하기 위해 미리 계산해둬야 할 것들이 있음.
  // Ex) 다항식 곱셈을 빠르게 하기 위한 준비
  SEALContext context(parms);

  KeyGenerator keygen(context);
  auto secret_key = keygen.secret_key();
  PublicKey public_key;
  keygen.create_public_key(public_key);
  RelinKeys relin_keys;
  keygen.create_relin_keys(relin_keys);


  Encryptor encryptor(context, public_key);
  Evaluator evaluator(context); 
  // evaluator 안의 method(함수)들로 연산을 수행할 것임 
  Decryptor decryptor(context, secret_key);
  BatchEncoder batch_encoder(context);

  auto N = poly_modulus_degree;

  // x 의 암호문? 
  vector<uint64_t> real_plain(N); // int: signed 32-bit 정수
  real_plain[0] = 3;
  real_plain[1] = 5;
  // real_plain 안에 내용물 채워넣는 과정 필요
  Plaintext plain;
  batch_encoder.encode(real_plain, plain);
  Ciphertext ctxt;
  encryptor.encrypt(plain, ctxt);
  // ctxt -> [3, 5, 0, 0, 0, ..., 0] 를 암호화하고 있음

  // f(x) = (x+1)^2 * (x-1) 계산
  Ciphertext c1;
  vector<uint64_t> one_plain(N);
  one_plain[0] = 1; one_plain[1] = 1;
  Plaintext one_ptxt;
  batch_encoder.encode(one_plain, one_ptxt);
  evaluator.add_plain(ctxt, one_ptxt, c1);

  Ciphertext c2;
  evaluator.sub_plain(ctxt, one_ptxt, c2);

  evaluator.square_inplace(c1);
  // c1 이라는 암호문 안에는 (x+1)^2이 들어있음
  evaluator.relinearize_inplace(c1, relin_keys);
  // c1의 (다항식) 길이를 3에서 2로 낮춰줌
  evaluator.mod_switch_to_next_inplace(c1);
  // Lv 2의 (x+1)^2 암호문을 얻었음.

  evaluator.mod_switch_to_next_inplace(c2);
  // Lv 1에 있었던 c2를 Lv 2로 낮춰줬음.
  evaluator.multiply_inplace(c1, c2);
  // c1은 (x+1)^2 * (x-1)를 암호화하고 있음
  evaluator.relinearize_inplace(c1, relin_keys);

  // [3, 5, 0, 0, ...] 으로 시작해서
  // f(x) = (x+1)^2 * (x-1) 계산
  // c1을 복호화하고 나면 [32, 144, 0, 0, 0, 0, ...]?
  Plaintext decrypted;
  decryptor.decrypt(c1, decrypted);
  vector<uint64_t> real_dec(N);
  batch_encoder.decode(decrypted, real_dec);

  cout << real_dec[0] << ", " << real_dec[1] << ", " << real_dec[2] << endl;
}
