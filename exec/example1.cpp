#include "seal/seal.h"
#include "utils.h"

using namespace std;
using namespace seal;

int main(int argc, char** argv) {
  EncryptionParameters parms(scheme_type::bfv);
  size_t poly_modulus_degree = 8192;
  auto modulus =
      PlainModulus::Batching(poly_modulus_degree, 30);

  parms.set_poly_modulus_degree(poly_modulus_degree);
  parms.set_coeff_modulus(CoeffModulus::BFVDefault(poly_modulus_degree));
  // BFVDefault = {43, 43, 44, 44, 44}
  // parms.set_coeff_modulus(CoeffModulus::Create(poly_modulus_degree, {40, 34, 55, 50}));
  parms.set_plain_modulus(modulus);

  SEALContext context(parms);

  KeyGenerator keygen(context);
  auto secret_key = keygen.secret_key();
  PublicKey public_key;
  keygen.create_public_key(public_key);
  RelinKeys relin_keys;
  keygen.create_relin_keys(relin_keys);

  Encryptor encryptor(context, public_key);
  Evaluator evaluator(context);
  Decryptor decryptor(context, secret_key);
  BatchEncoder batch_encoder(context);
  
  vector<uint64_t> plain_vec(poly_modulus_degree);
  for (size_t i = 0; i < plain_vec.size(); i++) {
    plain_vec[i] = random_uint64() % modulus.value();
  }
  plain_vec[0] = 3;

  Plaintext plain;
  batch_encoder.encode(plain_vec, plain);
  Ciphertext x;
  encryptor.encrypt(plain, x);

  // Let's evaluate x^3 + 7x^2 + 4x  
  // ---------- 1) x^2 ----------
  Ciphertext x2 = x;
  evaluator.square_inplace(x2);
  evaluator.relinearize_inplace(x2, relin_keys);
  // 에러 관리를 위해 레벨을 낮춰줌
  evaluator.mod_switch_to_next_inplace(x2);
  
  // ---------- 2) x^3 = x^2 * x ----------
  Ciphertext x3;
  
  // x를 x^2와 같은 레벨로 맞춰야 곱셈이 가능함
  Ciphertext x_l1 = x;
  evaluator.mod_switch_to_next_inplace(x_l1);

  evaluator.multiply(x2, x_l1, x3);
  evaluator.relinearize_inplace(x3, relin_keys);
  
  // ---------- 3) 상수 7, 4를 batching 형태로 encode ----------
  std::vector<uint64_t> vec7(poly_modulus_degree, 7ULL);
  std::vector<uint64_t> vec4(poly_modulus_degree, 4ULL);

  Plaintext p7, p4;
  batch_encoder.encode(vec7, p7);
  batch_encoder.encode(vec4, p4);

  // ---------- 4) 7x^2, 4x ----------
  Ciphertext term2, term3;
  evaluator.multiply_plain(x2, p7, term2);     // 7 * x^2
  evaluator.multiply_plain(x_l1, p4, term3);   // 4 * x

  // ---------- 5) 결과: x^3 + 7x^2 + 4x ----------
  
  Ciphertext result;
  evaluator.add(x3, term2, result);
  evaluator.add_inplace(result, term3);

  // result 가 최종 ciphertext
  Plaintext dec_ptxt;
  vector<uint64_t> dec_vec(poly_modulus_degree);
  decryptor.decrypt(result, dec_ptxt);
  batch_encoder.decode(dec_ptxt, dec_vec);
  
  cout << dec_vec[0] << " v.s. " << 102 << endl;

  return 0;
}
