#include "utils.h"
#include "seal/seal.h"

using namespace std;
using namespace seal;

int main(int argc, char** argv)
{
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
     
     // SEAL은 암호문을 String으로 입출력 (Serialization) 하는 API를 제공
     // Real World 프로토콜 설계할 때 반드시 필요함
     // 그런데 아마 다른 library들도 다 있을 것 같은 … ?
          
     stringstream ss;
     load_file_to_stringstream("my_ctxt_pub", ss);
     Ciphertext ctxt;
     ctxt.load(context, ss);  

     return 0;
}
