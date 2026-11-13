# SEAL 동형암호 실습 계획

2026-11-09 작성, 2026-11-13 스냅샷 (작성 중인 계획)

## 개요

정보보호 데이터분석(2026 가을) SEAL 실습은 2.5시간 × 2회, C++ SEAL 4.7.2(HEXL 켬)로 진행한다. 1회차 실험 0\~4 파일은 완성, 2회차는 설계 전이다.

**목표**

1. 라이브러리로 암호화–연산–복호화 흐름을 직접 짠다 (파라미터, 인코딩, 키, relinearize/rescale 위치).
2. 수업의 에러 공식이 실제 측정값과 맞는지 직접 확인한다.

**형식**

- 실험 0 은 인자 `___` 빈칸 채우기 (빈칸이 남으면 컴파일 에러). 실험 1 부터는 키 생성까지 주고, 단계별 주석 아래 연산 코드를 학생이 직접 쓴다. 이론식은 채워서 준다.
- 각 실험 끝에 `print_compare` 로 이론값(강의 식) · 측정값 · 차이를 출력한다.
- 교수가 빈칸을 짚으며 설명 → 학생이 채우고 실행 → 결과를 같이 읽는다. 워크시트(예측 표)는 쓰지 않는다.
- 주석은 순한글, 프로그램 출력은 영어(SEAL 함수명과 대조하기 쉽게).

**환경과 파일 구조**

| 위치 | 내용 |
| --- | --- |
| `infosec/SEAL`, `infosec/seal-install` | SEAL 소스, 설치본 |
| `infosec/<학번>/` (31개) | 학생 디렉토리. CMakeLists, README, `exec/` |
| `infosec/_template/exec/` | 학생용 파일 원본: `common.h`, `sanitycheck.cpp`, `expN_*.cpp` |
| `infosec/_template/solutions/` | 정답 코드 (나중에 배포) |

빌드: `cmake -S . -B build` → `cmake --build build --target expN_이름` → `./build/expN_이름`. C++20, `-O2 -flto -mavx512f -mavx512dq -mavx512vl`.

`common.h` 함수: `print_params`(n, 소수별 비트, log Pq, t) · `print_ct`(예산, size, level 출력 + 예산 반환) · `print_compare`(이론/측정/차이) · `print_check`(복호화 OK/FAIL) · `mulmod` · `Timer`.

## 1회차: BFV/BGV 노이즈와 회전 (실험 0\~4)

필수 분량 약 150분 (2.5시간을 꽉 채움, 여유 없음), 남는 시간은 실험별 추가 과제로 흡수한다. 실측값은 모두 서버에서 정답 코드로 돌린 값이다 (실행마다 1\~2비트 흔들림).

| 실험 | 파일 | 내용 | 시간 | 상태 |
| --- | --- | --- | --- | --- |
| 도입 | `—` | 서버 접속(s<학번>, 비밀번호 변경 권장), cmake 설정, 실습 방식 설명 | 10분 | 완료 |
| 0 | `exp0_params` | 파라미터 읽기, fresh 예산, 최대 log Pq | 15분 | 완료 |
| 1 | `exp1_noise` | 연산별 예산 감소 + relinearize, t = 20/40 | 30분 | 완료 |
| 2 | `exp2_modswitch` | BGV/BFV × mod switch 유무 | 30분 | 완료 |
| 3 | `exp3_lazy` | eager vs lazy relinearize (k = 1\~32), special prime | 35분 | 완료 |
| 4 | exp4\_rotation | rotate\_rows/columns, k칸 회전 (k = 1\~4095) 키 전략 3가지 | 30분 | 완료 |

### 실험 0: 파라미터 읽기

BGV, n = 8192, q = BFVDefault (43, 43, 44, 44 + P 44), t = 20비트. fresh 예산 146. MaxBitCount = 109 / 218 / 438 / 881 = 중간고사 8번 표.

- 빈칸: n, BFVDefault 인자, Batching 인자, Encryptor/Decryptor 키, encode/encrypt/decrypt 인자.
- 짚을 점: 마지막 소수 = special prime P, t ≡ 1 (mod 2n) 이어야 배칭(중간고사 5번), 예산 ≈ log q − log t − 에러비트 (174 − 20 − 8), level 3 = RNS 소수 4개.
- 선택 시연: q 를 {60,60,60,60} 으로 키우면 "not compliant with HomomorphicEncryption.org security standard" 로 거부. 슬라이드 논문 표(106/214/430)와 다른 건 기준 표가 다르기 때문.

### 실험 1: 연산별 노이즈

같은 파라미터, `t_bits` 를 20 → 40 으로 바꿔 두 번 실행.

| 단계 | 이론 (log n → log n / 2 보정) | t = 20 측정 | t = 40 측정 |
| --- | --- | --- | --- |
| ct + ct | 1 | 1 | 0 |
| ct × pt (벡터 b) | log t + log n / 2 (26.5 / 46.5) | 24\~25 | 44\~45 |
| ct × ct | e0 + log t + log n / 2 (35 / 55) | 35\~37 | 55\~56 |
| relinearize | (key switching 항만) | 0 | 0 |
| (ct × ct)² | e1 + log t + log n / 2 (71 / 110) | 73\~76 | 69 (바닥, FAIL) |

- 학생이 인코딩·암호화와 각 연산 코드를 직접 쓴다 (상수 3 곱하기는 수업에서 안 다뤘어서 삭제). 이론식은 채워서 준다.
- 짚을 점: 예산 감소량 = 에러 비트 증가량. 코드의 이론식은 강의 식의 log n 을 log n / 2 로 보정한 것 (worst-case n → 실제 √n, 부호가 랜덤). 이 보정은 수업에서 말로 설명. t 차이 20비트 = 감소량 차이. relinearize 0비트 = P 가 충분히 커서 key switching 에러가 묻힘 (fresh 에 회전하면 3비트 → 실험 4). t = 40 에서 두 번째 곱셈 FAIL.
- 추가 과제: `scheme_type::bfv` 로 바꿔 보기 (곱셈마다 32로 일정).

### 실험 2: Modulus switching

n = 16384, q = 36비트 × 7, P = 60, t = 20. 같은 암호문을 4번 제곱.

| 경우 | 곱셈마다 감소량 | switch 비용 | 결과 |
| --- | --- | --- | --- |
| BGV, switch 없음 | 37 → 77 → 바닥 (이론 36 → 73 → 151) | — | FAIL (depth 2) |
| BGV, 매번 switch | 36 → 37 → 40 → 46 (이론 36 → 37 → 40 → 49) | 0\~1 | OK |
| BFV, switch 없음 | 33 → 33 → 34 → 34 (이론 33.6, 강의 식 47.6 에서 log n 두 번 보정) | — | OK |
| BFV, 매번 switch | 33 → 33 → 33 → 33 | switch 단계에서 3씩 (36비트 소수를 버려도 에러도 같이 줄어 반올림분만 손해). 최종 예산 80 vs switch 없음 90 | OK |

- 학생이 인코딩·암호화, 제곱 + relinearize, mod switch 코드를 직접 쓴다. 이론식은 채워서 준다 (BGV `e + log_t + log_n` / 2, BFV `log_t + 2 * (log_n / 2)`).
- 짚을 점: switch 직후 예산은 그대로 (에러/q 비율을 줄이는 기술이 아님), BGV 의 이득은 다음 곱셈에서. BGV switch 해도 36 → 46 으로 커지는 건 곱셈 한 번에 늘어나는 에러(약 37)보다 RNS 소수(36)가 작아, 남은 에러가 매번 두 배로 불어나서 (switch 후 에러 10 → 15 → 26 → 48).
- 추가 과제: RNS 소수를 40비트로 바꿔 보기 (switch 후 에러가 매번 9비트로 돌아오고 감소량 36으로 일정).

속도 비교 (곱셈 + relinearize 1회, 각 레벨에서 20회 평균): switch 를 하면 레벨이 내려갈수록 곱셈이 빨라진다. BFV 는 에러 면에서는 손해지만 성능 면에서 이득 (3강 12쪽).

| 경우 | 곱셈 1\~4회 시간 (ms) | 합계 |
| --- | --- | --- |
| BFV, switch 없음 | 6.2 → 6.2 → 6.2 → 6.2 | 24.8 ms |
| BFV, 매번 switch | 6.3 → 5.2 → 3.9 → 3.3 | 18.7 ms |
| BGV, 매번 switch | 2.7 → 2.1 → 1.6 → 1.2 | 7.5 ms |

### 실험 3: Lazy relinearize + special prime

BGV, n = 16384, q = 60비트 RNS 소수 6개, t = 20. k 쌍의 Σ AᵢBᵢ 를 eager(곱셈마다 relinearize) / lazy(크기 3 암호문을 다 더한 뒤 1회) 로 계산하고, k = 1, 2, 4, 8, 16, 32 를 표 하나로 출력한다. special prime P = 60 과 18 에서 비교한다. 중간고사 3번. 아래 숫자는 2026-11-13 정답 코드 실측.

| k | P = 60 예산 eager / lazy | P = 18 예산 eager / lazy | P = 18 lazy − eager 이론 / 측정 | 시간 eager / lazy (ms, P = 60) |
| --- | --- | --- | --- | --- |
| 1 | 294 / 294 | 285 / 285 | 0 / 0 | 2.6 / 2.7 |
| 2 | 293 / 293 | 285 / 285 | 1 / 0 | 5.0 / 2.9 |
| 4 | 292 / 292 | 284 / 285 | 2 / 1 | 9.9 / 3.5 |
| 8 | 291 / 291 | 283 / 285 | 3 / 2 | 19.6 / 4.7 |
| 16 | 290 / 290 | 282 / 285 | 4 / 3 | 39.0 / 7.2 |
| 32 | 289 / 289 | 281 / 285 | 5 / 4 | 77.6 / 12.5 |

relinearize 1회 예산 비용은 P = 60 에서 0, P = 18 에서 11 (296 → 285). 이론 lazy − eager = log₂(k(M+K)/(kM+K)) 이고, P = 60 에서는 모든 k 에서 0 이다.

- 학생이 `eager_sum` 함수 본문(곱셈, relinearize, 더하기)과 `lazy_sum` 함수 본문(곱셈, 더하기 + 마지막 relinearize)을 직접 쓴다. 이론식은 채워서 준다 (M = 곱셈 1회 에러, K = key switching 1회 에러를 relinearize 전후 예산으로 추정). R = 5 회 반복 평균 시간.
- 짚을 점: 시간은 k 가 클수록 lazy 가 더 빠르다 (k = 8 에서 약 4배, k = 32 에서 약 6배) (BGV 곱셈은 싸고 relinearize 가 비용 대부분). 에러 차이는 P = 60 에서는 key switching 에러가 곱셈 에러에 묻혀 0, P 를 RNS 소수보다 한참 작게 해야 드러난다 → SEAL 이 마지막 소수를 가장 크게 두라는 이유.
- 측정이 이론보다 1 정도 작은 건 (k = 32 에서 이론 5, 측정 4), k 개의 key switching 에러가 랜덤 부호라 합이 k 배가 아니라 √k 배 정도로 커지기 때문 (실험 1 의 worst-case 식과 같은 이유).
- 함정: P 를 20비트로 두면 t 와 같은 소수가 골라져 파라미터 에러.

### 실험 4: Rotation 과 회전 키 전략

BGV, n = 8192, 기본 q, t = 20. 슬롯에 0 \~ 8191 을 넣고 (윗줄 0\~4095, 아랫줄 4096\~8191) 회전한다. 4강, 중간고사 6·7번.

1. 슬롯 구조: `rotate_rows(1)` 은 각 행을 왼쪽 1칸, `rotate_columns` 는 두 행 교환. 회전 1회 예산 비용 3비트 (146 → 143).
2. k칸 회전을 k = 1, 183, 1000, 2047, 3000, 4095 에 대해 회전 키 전략 3가지로 비교 (183 = 중간고사 7번). 2026-11-13 정답 코드 실측:

| 전략 | 키 개수 | 키 크기 | key switching 횟수 | 시간 (k 6개 평균) |
| --- | --- | --- | --- | --- |
| (a) 1칸 키 하나, 1칸을 k번 | 1 | 2.7 MB | k (183 → 183) | 820 ms (k = 4095 에서 1.95 s) |
| (b) SEAL 기본 (2의 거듭제곱) | 24 | 63 MB | 1\~4 (183 = 256 − 64 − 8 − 1 → 4) | 1.2 ms |
| (c) BSGS (B = 64: 1\~63, 64·1\~64·63) | 126 | 330 MB | 최대 2 (k = 64·g + b) | 0.95 ms |

- 학생 작성: rotate\_rows / rotate\_columns 호출, (a) 키 생성과 반복문, (b) 회전 호출, (c) BSGS step 목록(B = 64) 만들기 + 키 생성 + k 를 64·g + b 로 나눠 회전 2번. 전략별 key switching 횟수는 채워서 준다 (b = 183 = 256 − 64 − 8 − 1, 4번. 수업에서는 NAF 대신 "이진법처럼 쪼개되 빼기도 쓴다"로 설명).
- 짚을 점: 시간은 key switching 횟수에 비례 (1회당 약 0.5 ms). 키를 많이 두면 빨라지고 키 크기가 커진다 (4강 15쪽). 전체 회전을 다 커버하는 BSGS 는 키 약 2√N 개라 n = 8192 에서 126개 330 MB, n = 2¹⁴ 에서 3.5 GB → 실제로는 필요한 회전만 키를 만든다.
- SEAL 은 키가 없는 step 을 2의 거듭제곱으로 분해하므로, BSGS 키만으로 `rotate_rows(183)` 을 부르면 256칸 키를 찾다가 예외. 그래서 k 를 64·g + b 로 직접 나눈다 (183 = 128 + 55). 기본 Galois 키는 BSGS 가 아니다 (4강 "SEAL 이 BSGS 로 하는듯" 정정).

## 2회차: CKKS (실험 5\~7, 설계 전)

아직 파일을 만들지 않았다. 아래는 원래 계획서 기준 초안이며, 실험 3 이후 함께 정한다.

| 실험 | 내용 | 시간 | 연결 |
| --- | --- | --- | --- |
| 5 | CKKS 기본: πx³ + 0.4x + 1, rescale 누락 / 레벨 불일치 예외 보고 고치기, scale 2³⁰ vs 2⁴⁰ | 40분 | 5강 |
| 6 | 시그모이드 근사 σ(x) ≈ 0.5 + 0.197x − 0.004x³, 평문 vs 암호문 오차 | 25분 | — |
| 7 | MNIST 로지스틱 회귀 추론 (0 vs 1): 슬롯별 곱 → 회전-합 → b → 다항식. n = 2¹⁴, \[60, 40, 40, 40, 40, 60\] | 50분 | 4강 coefficient embedding (추가) |

- CKKS 는 노이즈 예산 함수가 없다 (`print_ct` 못 씀). 측정값은 복호화 오차 → common.h 에 오차 출력 함수 필요.
- 실험 7 재료: SEAL\_TEST lecture 브랜치의 `ckks_logi_reg.cpp`, `mnist_model.h`, `python_helper/train.py`.

## 배포와 운영

학생 계정 s<학번> 31개 생성 완료 (홈 = 학번 디렉토리, 초기 비밀번호는 별도 관리). 1회차 파일(exp0\~4, common.h, CMakeLists, README)은 `_admin/deploy.sh` 로 배포 완료. 새 실험은 `_template` 에 넣고 `EXECUTABLES` 에 이름을 추가한 뒤 `bash _admin/deploy.sh --apply`.

`EXECUTABLES` 에는 모든 실험을 등록해 둔다. 학생은 `--target` 으로 실험별로 빌드하므로 빈 실험이 있어도 문제없다. sanitycheck 는 한때 삭제했다가 빌드 환경 확인용으로 다시 넣었다 (EXECUTABLES 첫 줄).

- 배포 대상: `_template` 의 CMakeLists, README, `exec/` 전부. `solutions/` 는 빼고, 각 회차가 끝난 뒤 학생들에게 따로 나눠준다.
- 학생이 고친 파일은 덮어쓰지 않는다 (새 파일만 추가하는 스크립트).
- `-DSOL=ON`: `solutions/<이름>.cpp` 가 있는 실험은 정답으로 빌드. 빌드 폴더는 `build-sol` 로 따로 둔다 (CMake 가 옵션을 폴더에 기억).
- 빈칸이 남은 실험이 있어도 다른 실험은 `--target` 으로 따로 빌드된다.
- 새 실험 추가 = `exec/` 에 파일 + CMakeLists 의 `EXECUTABLES` 에 이름 한 줄.

* README 는 실험별 `--target` 빌드만 안내한다. `cmake --build build` 로 전부 빌드하면 미완성 실험 하나 때문에 전체가 멈춘다.
* 코드를 안 채운 채 실행하면 `print_ct` / `print_check` 가 "\[오류\] '라벨' 에서 쓴 암호문이 비어 있습니다" 를 내고 종료한다 (예전에는 메시지 없이 core dump).

## 과제와 남은 일

실습 자체는 완성 여부만 보고, 본 과제는 실습 뒤 동형암호 기반 PSI 수업 때 정한다.

- 후보: r · ∏(y − x) 단순 PSI 에서 시작해, 주어진 집합 크기에서 정확성을 유지하며 시간·통신량을 줄이고 측정값으로 설계를 설명하게 한다. 구두 확인을 곁들일지 도 그때 결정.
- 관련 문헌: Chen–Laine–Rindal (CCS'17), CHLR18 (labeled PSI), Cong 외 (CCS'21) = Microsoft APSI (SEAL 기반).

**남은 일**

- [x] 실험 3 (lazy relinearize + special prime) 파일
- [ ] 2회차 실험 5\~7 설계와 파일
- [ ] CKKS 용 오차 출력 함수 (common.h)
- [ ] 실험별 추가 과제 문구 파일에 넣을지 결정
- [x] 31개 디렉토리 배포 (sanitycheck/README 한글판 포함)
- [ ] 회차별 solutions 배포
