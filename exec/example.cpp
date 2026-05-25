#include "seal/seal.h"
#include "utils.h"

using namespace std;
using namespace seal;

int main(int argc, char** argv) {
  size_t poly_modulus_degree = 4096;
  EncryptionParameters parms(scheme_type::bfv);
  parms.set_poly_modulus_degree(poly_modulus_degree);
  parms.set_coeff_modulus(CoeffModulus::BFVDefault(poly_modulus_degree));
  parms.set_plain_modulus(
      PlainModulus::Batching(poly_modulus_degree, 20));

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
}
