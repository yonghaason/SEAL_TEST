#pragma once
// PSI 실험 (실험 7 ~ 9) 과 해싱 시연에서 쓰는 함수들. 학생이 고칠 곳은 없다.
//
// 원소는 모두 Z_t 의 값 (2 이상 t 미만) 이다. 0 과 1 은 빈칸 표시로 쓴다.
//   - 받는 쪽 (receiver) 의 빈 슬롯: 0
//   - 보내는 쪽 (sender) 의 bucket 을 채우는 가짜 원소: 1
//   두 값은 서로 다르고 실제 원소와도 겹치지 않으므로 빈칸끼리 일치로 잡히지 않는다.

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <random>
#include <set>
#include <sstream>
#include <unordered_set>
#include <utility>
#include <vector>
#include "common.h"

const uint64_t EMPTY_SLOT = 0;  // 받는 쪽의 빈 슬롯
const uint64_t PAD_ITEM = 1;    // 보내는 쪽 bucket 을 채우는 가짜 원소

// 64비트 해시 (splitmix64 의 마무리 단계)
inline uint64_t mix64(uint64_t x)
{
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

// i 번째 해시 함수: 원소 x 를 테이블 크기 N 의 칸 번호로 보낸다.
inline size_t hash_bucket(int i, uint64_t x, size_t N)
{
    return mix64(x * 0x9e3779b97f4a7c15ULL + 0x632be59bd9b4e019ULL * (i + 1)) % N;
}

// 서로 다른 원소 count 개를 [2, t) 에서 뽑는다. used 에 있는 값은 피하고, 뽑은 값은 used 에 넣는다.
inline std::vector<uint64_t> random_items(size_t count, uint64_t t, std::mt19937_64 &rng,
                                          std::unordered_set<uint64_t> &used)
{
    std::vector<uint64_t> out;
    while (out.size() < count)
    {
        uint64_t v = 2 + rng() % (t - 2);
        if (used.insert(v).second)
        {
            out.push_back(v);
        }
    }
    return out;
}

// 근 roots 를 갖는 다항식 prod (y - x) 의 계수 (mod t). 반환값 c 에 대해 p(y) = c[0] + c[1] y + ... + c[d] y^d.
inline std::vector<uint64_t> poly_from_roots(const std::vector<uint64_t> &roots, uint64_t t)
{
    std::vector<uint64_t> c(1, 1);
    for (uint64_t x : roots)
    {
        std::vector<uint64_t> next(c.size() + 1, 0);
        for (size_t d = 0; d < c.size(); d++)
        {
            next[d + 1] = (next[d + 1] + c[d]) % t;               // y * c[d] y^d
            next[d] = (next[d] + t - mulmod(c[d], x % t, t)) % t;  // -x * c[d] y^d
        }
        c.swap(next);
    }
    return c;
}

// y^e mod t
inline uint64_t powmod(uint64_t y, uint64_t e, uint64_t t)
{
    uint64_t r = 1;
    for (uint64_t i = 0; i < e; i++)
    {
        r = mulmod(r, y, t);
    }
    return r;
}

// ---------------------------------------------------------------
// 거듭제곱 계획: 받는 쪽이 보낸 거듭제곱 (sent) 으로 y^1 ~ y^D 를 만드는 방법.
//   split[k] = (a, b): y^k = y^a * y^b 로 만든다 (보낸 거듭제곱이면 (0, 0)).
//   depth[k]: y^k 의 곱셈 깊이 (보낸 것은 0).
// (a, b) 는 실험 8 에서 학생이 쓰는 split_power 가 정한다. make_plan 은 그것을 모아 깊이와 곱셈 수를 세고,
// 잘못된 (a, b) 를 걸러낸다.
// ---------------------------------------------------------------
struct PowerPlan
{
    std::vector<std::pair<int, int>> split;
    std::vector<int> depth;
    int max_depth = 0;
    int mult_count = 0;
};

using SplitFn = std::pair<int, int> (*)(int, const std::vector<int> &);

inline PowerPlan make_plan(const std::vector<int> &sent, int D, SplitFn split_power)
{
    std::set<int> got;
    for (int s : sent)
    {
        if (s >= 1 && s <= D)
        {
            got.insert(s);
        }
    }
    if (!got.count(1))
    {
        std::cout << std::flush;
        std::cerr << "\n[오류] 보낸 거듭제곱에 1 (y 자체) 이 없습니다.\n";
        std::exit(1);
    }
    PowerPlan plan;
    plan.split.assign(D + 1, { 0, 0 });
    plan.depth.assign(D + 1, 0);
    for (int k = 1; k <= D; k++)
    {
        if (got.count(k))
        {
            continue;
        }
        auto [a, b] = split_power(k, sent);
        if (a < 1 || b < 1 || a + b != k)
        {
            std::cout << std::flush;
            std::cerr << "\n[오류] split_power(" << k << ") 가 (" << a << ", " << b << ") 를 돌려줬습니다. "
                      << "a + b = " << k << " 이고 a, b 는 1 이상이어야 합니다. split_power 를 채웠는지 확인하세요.\n";
            std::exit(1);
        }
        plan.split[k] = { a, b };
        plan.depth[k] = std::max(plan.depth[a], plan.depth[b]) + 1;
        plan.max_depth = std::max(plan.max_depth, plan.depth[k]);
        plan.mult_count++;
    }
    return plan;
}

// ---------------------------------------------------------------
// 해싱 (실험 9, 해싱 시연)
// ---------------------------------------------------------------

// Cuckoo hashing: 받는 쪽 원소를 해시 함수 h 개로 테이블 (크기 N) 에 한 칸씩 넣는다.
// 원소 y 는 h_0(y), ..., h_{h-1}(y) 중 한 칸에 들어간다. 빈칸이 없으면 아무 칸의 원소를 밀어내고 그 원소를 다시 넣는다.
// 성공하면 table (빈칸은 EMPTY_SLOT) 을 채우고 true, max_kicks 번 밀어내도 안 되면 false.
inline bool cuckoo_insert(const std::vector<uint64_t> &items, size_t N, int h, std::mt19937_64 &rng,
                          std::vector<uint64_t> &table, int max_kicks = 500)
{
    table.assign(N, EMPTY_SLOT);
    for (uint64_t y : items)
    {
        uint64_t cur = y;
        int kicks = 0;
        while (true)
        {
            bool placed = false;
            for (int i = 0; i < h; i++)
            {
                size_t b = hash_bucket(i, cur, N);
                if (table[b] == EMPTY_SLOT)
                {
                    table[b] = cur;
                    placed = true;
                    break;
                }
            }
            if (placed)
            {
                break;
            }
            if (++kicks > max_kicks)
            {
                return false;
            }
            size_t b = hash_bucket(static_cast<int>(rng() % h), cur, N);
            std::swap(cur, table[b]);
        }
    }
    return true;
}

// Simple hashing: 보내는 쪽 원소 x 를 h_0(x), ..., h_{h-1}(x) 칸 모두에 넣는다 (같은 칸이 겹치면 한 번만).
// 받는 쪽 y 가 어느 해시 함수로 들어갔든, y = x 이면 그 칸에 x 가 있다.
inline std::vector<std::vector<uint64_t>> simple_hash(const std::vector<uint64_t> &items, size_t N, int h)
{
    std::vector<std::vector<uint64_t>> bucket(N);
    for (uint64_t x : items)
    {
        std::set<size_t> bs;
        for (int i = 0; i < h; i++)
        {
            bs.insert(hash_bucket(i, x, N));
        }
        for (size_t b : bs)
        {
            bucket[b].push_back(x);
        }
    }
    return bucket;
}

inline size_t max_bucket_size(const std::vector<std::vector<uint64_t>> &bucket)
{
    size_t m = 0;
    for (const auto &b : bucket)
    {
        m = std::max(m, b.size());
    }
    return m;
}
