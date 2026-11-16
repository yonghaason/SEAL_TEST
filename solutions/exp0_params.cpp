//   빌드: cmake --build build --target exp0_params
//   실행: ./build/exp0_params
#include "common.h"

using namespace std;
using namespace seal;

int main()
{
    // 암호화 파라미터: 스킴, n, q (coeff_modulus), t (plain_modulus)
    EncryptionParameters parms(scheme_type::bgv);
    size_t n = 8192;
    parms.set_poly_modulus_degree(n);
    parms.set_coeff_modulus(CoeffModulus::BFVDefault(n));
    parms.set_plain_modulus(PlainModulus::Batching(n, 20));

    SEALContext context(parms);
    print_params(context);
    // cout << context.parameter_error_message() << endl;

    KeyGenerator keygen(context);
    SecretKey secret_key = keygen.secret_key();
    PublicKey public_key;
    keygen.create_public_key(public_key);

    Encryptor encryptor(context, public_key);
    Decryptor decryptor(context, secret_key);
    BatchEncoder encoder(context);

    vector<uint64_t> m(encoder.slot_count(), 0);
    for (size_t i = 0; i < m.size(); i++)
    {
        m[i] = i;
    }
    Plaintext pt;
    encoder.encode(m, pt);
    Ciphertext ct;
    encryptor.encrypt(pt, ct);
    print_ct("fresh", ct, decryptor, context);

    Plaintext pt_dec;
    decryptor.decrypt(ct, pt_dec);
    vector<uint64_t> m_dec;
    encoder.decode(pt_dec, m_dec);
    cout << "decrypted: " << m_dec[0] << ", " << m_dec[1] << ", " << m_dec[2] << ", ...\n\n";

    wait_enter("다음: n 별 최대 log2(Pq)");

    // 128비트 안전성에서 허용되는 최대 log2(Pq)
    for (size_t deg : { 4096, 8192, 16384, 32768 })
    {
        cout << "n = " << setw(5) << deg << ": max log2(Pq) = " << CoeffModulus::MaxBitCount(deg) << "\n";
    }

    return 0;
}
