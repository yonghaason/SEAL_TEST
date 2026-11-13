# 1회차 수업 진행 노트

실험마다 시작 전 → 학생 작업 → 결과 읽기 → 질문 순서. 숫자는 정답 코드 실측값이며 실행마다 1\~3비트 흔들린다. 정답 빌드: `cmake -S . -B build-sol -DSOL=ON` (\_template 에서).

## 수업 전 준비와 도입 (10분)

**수업 전 (교수)**

- `_template` 에서 정답 빌드를 미리 돌려 둔다: `cmake -S . -B build-sol -DSOL=ON && cmake --build build-sol`. 널널하게 5분.
- 강의자료 2강 21·25쪽, 3강 3·11·12쪽, 4강 12·15쪽, 중간고사 3·4·6·7·8번을 탭에 띄워 둔다.

**도입에서 말할 것**

1. 접속: 계정 `s<학번>`, 초기 비밀번호로 접속. **바로 `passwd` 로 바꾸라고 말하기**.
2. 처음 한 번: `cmake -S . -B build`. 이후 실험마다 `cmake --build build --target expN_이름` → `./build/expN_이름`. 전체 빌드(`cmake --build build`)는 하지 말 것.
3. 실습 방식: 실험 0 은 빈칸(`___`) 채우기, 실험 1 부터는 주석 아래에 연산 코드를 직접 쓴다. 끝에 theory / measured / diff 가 출력되는데 이게 오늘의 핵심: **수업의 에러 공식이 실제로 맞는지 본다.**
4. 예산(noise budget)은 비밀키로만 잔 수 있는 디버깅용 값. 서버는 못 본다.
5. 오류 메시지 "\[오류\] 'xxx' 에서 쓴 암호문이 비어 있습니다" = 아직 안 채운 코드가 있다는 뜻.

## 실험 0: 파라미터 읽기 (15분)

**시작 전 (3분)** — `exec/exp0_params.cpp` 를 화면에 띄우고 빈칸을 위에서부터 짚는다.

- `set_poly_modulus_degree` = n, `set_coeff_modulus` = q (큰 수 하나가 아니라 RNS 소수 목록, 3강), `set_plain_modulus` = t.
- 질문: "t 를 왜 `Batching(n, 20)` 으로 고르나?" → 배칭하려면 t ≡ 1 (mod 2n) (4강, 중간고사 5번).

**학생 작업 (5분)** — 빈칸 10개: `n = 8192`, `BFVDefault(n)`, `Batching(n, 20)`, Encryptor 에 `public_key`, Decryptor 에 `secret_key`, encode(`m`), encrypt(`pt`), decrypt(`ct`). `-Wfatal-errors` 라 에러가 하나씩 나온다.

**결과 읽기 (5분)**

```
coeff_modulus = { 43, 43, 44, 44, 44 } bits
log2(Pq) = 218  (log2 q = 174, special prime P = 44)
fresh   budget = 146 bits   size = 2   level = 3
n = 4096 ... 109 / 218 / 438 / 881
```

- 마지막 44 = special prime P (2강 key switching). 나머지 4개가 RNS 소수 → level 3, 2, 1, 0.
- **칠판에 쓸 식: 예산 ≈ log q − log t − (에러 비트) = 174 − 20 − 약 8.** 실험 1 전체의 기준. "예산 감소량 = 에러 비트 증가량".
- 109/218/438/881 = 중간고사 8번 표. log(Pq) 기준 (special prime 포함, 2강 27쪽).

**질문과 답**

- "t 를 40비트로 바꾸면 예산은?" → 직접 바꿔 돌리게. 126 (정확히 20 감소).
- "슬라이드 표(106/214/430)와 왜 다르냐?" → SEAL 은 2018 HE 표준 표, 슬라이드는 더 최근 가이드라인.
- (선택 시연) q 를 `{60,60,60,60}` 로 키우고 `parameter_error_message()` 주석을 풀면 "not compliant with HomomorphicEncryption.org security standard". 라이브러리가 안전성 표를 강제한다.

## 실험 1: 연산별 노이즈 (30분)

**시작 전 (3분)** — 목표: 덧셈, 평문곱, 암호문곱, relinearize 각각이 예산을 얼마나 쓰는지 재고 2강 공식과 비교. 쓸 함수: `encoder.encode`, `encryptor.encrypt`, `evaluator.add`, `multiply_plain`, `multiply`, `relinearize_inplace`.

**학생 작업 (12분)** — 인코딩·암호화 4줄, 각 연산 + 결과 암호문 선언 (`Ciphertext ct_add;` 등). 막히는 곳: 변수 선언을 빠뜨리면 "ct\_add was not declared" 컴파일 에러. 끝나면 `t_bits = 40` 으로 바꿔 한 번 더.

**결과 읽기 (10분)**

| 연산 | 이론 | t=20 측정 | t=40 측정 |
| --- | --- | --- | --- |
| ct + ct | 1 | 0\~1 | 0\~1 |
| ct × pt (b) | log t + log n/2 = 26.5 / 46.5 | 24\~25 | 44\~45 |
| ct × ct | e0 + log t + log n/2 = 35 / 55 | 35\~37 | 55\~56 |
| relinearize | — | 0 | 0 |
| (ct × ct)² | e1 + log t + log n/2 = 71 / 110 | 73\~76 | 바닥, FAIL |

- 덧셈은 거의 공짜, 곱셈이 비싼 것. t 를 20 → 40 으로 바꾸면 곱셈 감소량도 정확히 20 차이 (log t 항).
- 두 번째 곱셈은 첫 곱셈의 약 2배: BGV 에러 비트가 두 배로 (2강 22쪽).
- t = 40 에서 예산 0 → decrypt FAIL. "예산이 0 이면 결과가 틀린다".
- relinearize 0비트: P 가 충분히 커서 key switching 에러(fresh 수준)가 곱셈 에러에 묻힌다 → 실험 3 에서 다시.

**질문과 답**

- "코드 이론식은 왜 log n 이 아니라 log n / 2?" → 강의 식은 worst-case (다항식 곱셈에서 n 개 항이 같은 부호로 쌓임). 실제로는 부호가 랜덤이라 √n 배 → log n / 2. 강의 식을 넣으면 측정이 5\~9비트 작게 나온다.
- "에러 비트 e0 는 어떻게 구했나?" → log q − log t − 예산 (실험 0 의 식 거꾸로).
- "(ct×ct)² 는 왜 이론보다 3\~4 더 크나?" → 곱셈 결과끼리 곱하면 에러가 fresh 처럼 고르지 않아 √n 근사보다 3\~5비트 더 커진다 (실측 관찰).
- 추가 과제: `scheme_type::bfv` 로 바꿔 보기 → 곱셈마다 약 32 로 일정 (실험 2 예고).

## 실험 2: Modulus switching (30분)

**시작 전 (3분)** — 같은 암호문을 4번 제곱. BGV/BFV × switch 있음/없음 4가지를 자동으로 돌린다. 파라미터: n = 2¹⁴, RNS 소수 36비트 × 7, P = 60, t = 20. 쓸 함수: `square_inplace`, `relinearize_inplace`, `mod_switch_to_next_inplace`.

**학생 작업 (7분)** — 인코딩·암호화 2줄, 반복문 안 제곱 + relinearize 2줄, mod switch 1줄. 모두 변수 `ct` 하나에 제자리(`_inplace`).

**결과 읽기 (15분)**

| 경우 | 곱셈마다 감소량 | switch 비용 | 결과 | 곱셈 시간 합 |
| --- | --- | --- | --- | --- |
| BGV, switch X | 37 → 77 → 바닥 | — | FAIL (depth 2) | — |
| BGV, switch O | 36 → 39 → 45 → 53 | 0\~1 | OK | 7.5 ms |
| BFV, switch X | 33 → 33 → 33 → 33 | — | OK | 24.8 ms |
| BFV, switch O | 33 → 33 → 33 → 33 | 3씩 | OK | 18.7 ms |

순서대로 짚기:

1. **switch 직후 예산은 거의 그대로** (187 → 187). q 와 에러를 같이 나눔 → "에러/q 비율을 줄이는 기술이 아니다" (3강 12쪽).
2. **BGV 의 이득은 다음 곱셈에서**: switch 없으면 37 → 77 (두 배씩), 있으면 36 → 39 (거의 일정). depth 2 → 4.
3. **BFV 는 곱셈마다 33 으로 일정** (선형 성장, 3강 11쪽). switch 해도 곱셈 감소량은 그대로고 switch 마다 3비트 추가 손해 → 대신 레벨이 내려갈수록 곱셈이 빨라짐 (6.2 → 3.3 ms). BFV 의 switch 는 성능용.
4. BGV(2.7 ms) vs BFV(6.2 ms) 같은 레벨 곱셈: "BFV 는 곱셈 자체가 비싸다".

**질문과 답**

- "BFV 에서 switch 하면 왜 3비트 손해?" → switch 전 에러 41비트 ÷ 36비트 소수 = 5비트인데, switch 가 더하는 반올림 에러가 약 8비트(fresh 수준) → 합이 8 → 3비트 손해. BGV 는 곱셈 후 에러가 44\~46 으로 커서 반올림이 묻힌다.
- "BGV switch 해도 감소량이 점점 커지는 이유?" → RNS 소수 36비트가 곱셈 한 번의 에러 증가(약 37)보다 작아 switch 후 에러가 10 → 15 → 26 로 남고, 남은 에러가 다음 곱셈에서 두 배로 불어난다. 추가 과제: RNS 소수를 40비트로 바꾸면 에러가 매번 9 로 돌아오고 감소량 36 일정 (3강 14쪽 "qᵢ 를 log n + log t 근처로").
- "BFV 이론식은 왜 log t + 2·(log n/2)?" → 강의 식 log t + 2 log n 에 log n 이 두 번 있어 √n 보정도 두 번. 강의 식 그대로면 47.6 vs 측정 33.

## 실험 3: Lazy relinearize (35분)

**시작 전 (5분)** — 중간고사 3번을 띄우고 시작. k = 8 쌍의 Σ AᵢBᵢ 를 eager(곱셈마다 relinearize 후 더하기) / lazy(크기 3 암호문을 다 더한 뒤 relinearize 1번) 로 계산. special prime P = 60, 18 두 번 돌린다. 크기가 다른 암호문(2, 3)도 `add` 로 더해진다고 말해 주기.

**학생 작업 (12분)** — eager 반복문 본문(곱셈, relinearize, 더하기), lazy 반복문 본문(곱셈, 더하기) + 마지막 relinearize. 막히는 곳: `Ciphertext eager;` 는 0 의 암호문이 아니라 **빈 객체**라 바로 더하면 예외 → 첫 항(i == 0)은 대입, 이후 더하기. (또는 `encryptor.encrypt_zero(eager)` 로 시작)

**결과 읽기 (10분)**

|  | P = 60 | P = 18 |
| --- | --- | --- |
| relinearize 1회 비용 | 0 | 9\~11 |
| 예산 eager / lazy | 292 / 292 | 282 / 284 |
| lazy − eager (이론 / 측정) | 0 / 0 | 3 / 2 |
| 시간 eager / lazy | 20 / 4.8 ms | 19 / 4.6 ms |

1. **시간은 항상 lazy 가 약 4배** (중간고사 3(b)). BGV 곱셈(0.03 ms)보다 relinearize(0.5 ms)가 비용의 대부분.
2. **에러 차이는 P 에 달렸다** (중간고사 3(c)). P = 60 이면 key switching 에러 K 가 곱셈 에러 M 에 묻혀 0, P = 18 이면 K ≫ M 이라 log k = 3 에 가까워진다.
3. 이론식: eager 에러 = k(M + K), lazy = kM + K → 차이 log₂(k(M+K)/(kM+K)). K 는 "곱셈 1번 → relinearize" 에서 줄어든 예산으로 추정.
4. 결론: "그래서 SEAL 은 마지막 소수(special prime)를 가장 크게 잡으라고 한다" (3강 3쪽, P 로 나눠 c₃·e\_sw 를 줄이는 이유).

**질문과 답**

- "P = 18 에서 왜 정확히 3 이 안 나오나?" → 이론식은 에러가 k 배로 쌓이는 worst-case. 실제로는 부호가 섞여 조금 덜 커진다 (실험 1 의 √n 과 같은 이유). √k 로 보정하면 1.5 라 측정 2 는 그 사이.
- "P 를 20비트로 하면?" → SEAL 이 t 와 같은 소수를 골라 파라미터 에러. 18 또는 24 사용.
- "왜 P 를 작게 잡고 싶어 하나?" → 안전성은 log(Pq) 기준이라 P 를 줄이면 같은 n 에서 q 를 더 쓸 수 있다 (3강 5\~6쪽 dnum 트레이드오프).

## 실험 4: Rotation (30분)

**시작 전 (5분)** — 4강 12쪽 그림(2 × n/2 행렬)과 중간고사 6·7번을 띄운다. 슬롯에 0 \~ 8191 을 넣고 돌려 본다. 쓸 함수: `rotate_rows`, `rotate_columns`, `keygen.create_galois_keys(키)`, `create_galois_keys(vector<int>{...}, 키)`. 회전은 키 스위칭(s(x⁵) → s(x))이라 회전 키가 필요하다는 점 상기.

**학생 작업 (12분)**

- 파트 1: `rotate_rows(ct, 1, galois_keys, ct_rot)`, `rotate_columns(ct, galois_keys, ct_swap)`.
- 파트 2: (a) 1칸 키 생성 + 1칸 회전 183번, (b) 기본 키로 `rotate_rows_inplace(ct_b, 183, ...)`, (c) BSGS step 목록(1\~15, 16·1\~16·15) → 키 생성 → 176칸 + 7칸.
- 막히는 곳: (c) 에서 BSGS 키로 `rotate_rows(183)` 을 바로 부르면 SEAL 이 2의 거듭제곱(256칸) 키를 찾다 예외. 일부러 겪게 해도 좋다 → "직접 16·11 + 7 로 나눠야 한다".

**결과 읽기 (10분)**

```
[ 0, 1, 2, 3, ..., 4095 ]          rotate_rows(1)  -> [ 1, 2, 3, 4, ..., 0 ]
[ 4096, ..., 8191 ]                rotate_columns  -> 두 행 교환

strategy           keys  key MB  switches  budget drop     ms
(a) one key           1     2.7       183         5~9     90
(b) default (2^i)    24    63.0         4         3~5    2.2
(c) BSGS             30    78.7         2         3~4    1.2
```

1. 회전 1회 예산 비용 3비트 (146 → 143): fresh 암호문이라 key switching 에러가 드러난다 (실험 1 의 relinearize 0비트와 대비).
2. 시간은 key switching 횟수에 비례 (1회당 약 0.5 ms). 키를 많이 들면 빨라지고 키가 커진다 (4강 15쪽).
3. (b) SEAL 기본은 2의 거듭제곱 키 + 이진법처럼 쪼개기. 183 = 256 − 64 − 8 − 1 → 4번. 기본 키는 BSGS 가 아니다 (4강 "SEAL 이 BSGS 로 하는듯" 정정).
4. (c) 중간고사 7번 그대로: 183 = 16·11 + 7 → 2번.

**질문과 답**

- "183 은 이진법 10110111 이라 6번 아닌가?" → 256칸 돌리고 64, 8, 1칸 되돌리는 게 더 적다. 빼기도 쓰는 이진법.
- "그럼 모든 회전을 BSGS 로 커버하면?" → 키 약 2√N 개. n = 2¹⁴ 이면 182개, 3.5 GB (기본 키 26개 491 MB). 그래서 실제로는 필요한 회전만 키를 만든다 (예: 슬롯 합에는 1, 2, 4, … 만 → 2회차 MNIST).
- "(a) 의 예산 감소가 183번 했는데 왜 5\~9 뿐?" → key switching 에러는 더해질 뿐 곱해지지 않아 183번이면 약 log₂(183)/2\~log₂(183) 비트만 늘어난다.
