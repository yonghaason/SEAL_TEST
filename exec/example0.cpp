#include "seal/seal.h"
#include "utils.h"

using namespace std;
using namespace seal;

void test(scheme_type scheme, vector<uint64_t>& plain_vec, Modulus modulus) {
  EncryptionParameters parms(scheme);

  size_t poly_modulus_degree = 8192;
  parms.set_poly_modulus_degree(poly_modulus_degree);
  parms.set_coeff_modulus(CoeffModulus::BFVDefault(poly_modulus_degree));
  // BFVDefault = {43, 43, 44, 44, 44}
  // parms.set_coeff_modulus(CoeffModulus::Create(poly_modulus_degree, {40, 34, 55, 50}));
  parms.set_plain_modulus(
      PlainModulus::Batching(poly_modulus_degree, modulus.bit_count()));

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

  Plaintext plain;
  batch_encoder.encode(plain_vec, plain);
  Ciphertext encrypted;
  encryptor.encrypt(plain, encrypted);
  Ciphertext encrypted2;
  encryptor.encrypt(plain, encrypted2);

  cout << "    + Noise budget fresh:                   "
       << decryptor.invariant_noise_budget(encrypted) << " bits" << endl;
  Ciphertext square;
  evaluator.square(encrypted, square);
  evaluator.relinearize_inplace(square, relin_keys);
  cout << "    + Noise budget of square:         "
       << decryptor.invariant_noise_budget(square) << " bits" << endl;

  Ciphertext mult;
  evaluator.multiply(encrypted, encrypted2, mult);
  evaluator.relinearize_inplace(mult, relin_keys);
  cout << "    + Noise budget of mult:         "
       << decryptor.invariant_noise_budget(mult) << " bits" << endl;

  Ciphertext multP;
  evaluator.multiply_plain(encrypted, plain, multP);
  cout << "    + Noise budget of multP:         "
       << decryptor.invariant_noise_budget(multP) << " bits" << endl;
}

int main(int argc, char** argv) {
  size_t poly_modulus_degree = 8192;
  auto modulus =
      PlainModulus::Batching(poly_modulus_degree, std::stoi(argv[1]));
  vector<uint64_t> plain_vec(poly_modulus_degree);
  for (size_t i = 0; i < plain_vec.size(); i++) {
    plain_vec[i] = random_uint64() % modulus.value();
  }

  cout << "BFV" << endl;
  test(scheme_type::bfv, plain_vec, modulus);

  cout << "BGV" << endl;
  test(scheme_type::bgv, plain_vec, modulus);

  return 0;
}
