#pragma once

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include "seal/seal.h"

// 암호화 파라미터를 출력한다.
// coeff_modulus 의 마지막 소수는 special prime P 이다 (key switching 에서만 쓰인다).
inline void print_params(const seal::SEALContext &context)
{
    auto &parms = context.key_context_data()->parms();
    auto &coeff_modulus = parms.coeff_modulus();

    std::string scheme;
    switch (parms.scheme())
    {
    case seal::scheme_type::bfv: scheme = "BFV"; break;
    case seal::scheme_type::bgv: scheme = "BGV"; break;
    case seal::scheme_type::ckks: scheme = "CKKS"; break;
    default: scheme = "none";
    }

    int total_bits = 0;
    std::cout << "[" << scheme << "] n = " << parms.poly_modulus_degree() << "\n";
    std::cout << "  coeff_modulus = { ";
    for (size_t i = 0; i < coeff_modulus.size(); i++)
    {
        total_bits += coeff_modulus[i].bit_count();
        std::cout << coeff_modulus[i].bit_count() << (i + 1 < coeff_modulus.size() ? ", " : "");
    }
    int special_bits = coeff_modulus.back().bit_count();
    std::cout << " } bits\n";
    std::cout << "  log2(Pq) = " << total_bits << "  (log2 q = " << total_bits - special_bits
              << ", special prime P = " << special_bits << ")\n";
    if (parms.scheme() != seal::scheme_type::ckks)
    {
        std::cout << "  plain_modulus t = " << parms.plain_modulus().value() << " ("
                  << parms.plain_modulus().bit_count() << " bits)\n";
    }
}

// 암호문이 비어 있으면 (아직 만드는 코드를 안 채웠으면) 안내 메시지를 내고 종료한다.
inline void require_ct(const std::string &label, const seal::Ciphertext &ct)
{
    if (ct.size() == 0)
    {
        std::cout << std::flush;
        std::cerr << "\n[오류] '" << label << "' 에서 쓴 암호문이 비어 있습니다. "
                  << "이 암호문을 만드는 코드를 채웠는지 확인하세요.\n";
        std::exit(1);
    }
}

// BFV/BGV 암호문의 노이즈 예산, 크기, 레벨을 출력한다.
// size: 보통 암호문은 2, 곱셈 직후 (relinearize 전) 에는 3.
// level: modulus chain 에서의 위치. mod_switch_to_next 를 할 때마다 1 씩 줄어든다.
// 반환값: 노이즈 예산 (비트)
inline int print_ct(
    const std::string &label, const seal::Ciphertext &ct, seal::Decryptor &decryptor,
    const seal::SEALContext &context)
{
    require_ct(label, ct);
    int level = context.get_context_data(ct.parms_id())->chain_index();
    int budget = decryptor.invariant_noise_budget(ct);
    std::cout << std::left << std::setw(24) << label << std::right
              << " budget = " << std::setw(4) << budget << " bits"
              << "   size = " << ct.size() << "   level = " << level << "\n";
    return budget;
}

// 예산 감소량의 이론값과 측정값을 나란히 출력한다.
inline void print_compare(const std::string &label, double theory, int measured)
{
    std::cout << std::left << std::setw(16) << label << std::right << std::fixed << std::setprecision(1)
              << "  theory = " << std::setw(5) << theory << "   measured = " << std::setw(4) << measured
              << "   diff = " << std::setw(5) << measured - theory << "\n";
    std::cout.unsetf(std::ios::fixed);
}

// 암호문을 복호화해서 expected 와 모든 슬롯이 같은지 출력한다.
inline void print_check(
    const std::string &label, const seal::Ciphertext &ct, const std::vector<uint64_t> &expected,
    seal::Decryptor &decryptor, seal::BatchEncoder &encoder)
{
    require_ct(label, ct);
    seal::Plaintext pt;
    decryptor.decrypt(ct, pt);
    std::vector<uint64_t> got;
    encoder.decode(pt, got);
    std::cout << std::left << std::setw(24) << label << std::right
              << (got == expected ? " decrypt OK" : " decrypt FAIL") << "\n";
}

// (a * b) mod t. 40비트 t 에서도 넘치지 않도록 128비트로 계산한다.
inline uint64_t mulmod(uint64_t a, uint64_t b, uint64_t t)
{
    return static_cast<uint64_t>((static_cast<unsigned __int128>(a) * b) % t);
}

// Enter 를 누를 때까지 기다린다 (출력이 한꺼번에 지나가지 않도록). 터미널이 아니면 기다리지 않는다.
inline void wait_enter(const std::string &next = "")
{
    if (!isatty(fileno(stdin)))
    {
        return;
    }
    std::cout << "\n[Enter 를 누르면 계속" << (next.empty() ? "" : " -> " + next) << "]" << std::flush;
    std::string line;
    std::getline(std::cin, line);
    std::cout << "\n";
}

// 간단한 스톱워치. 사용법: Timer tm; (측정할 코드) double ms = tm.ms();
struct Timer
{
    std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
    double ms() const
    {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(now - start).count();
    }
};
