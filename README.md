# SEAL 동형암호 실습 (정보보호 데이터분석, 2026 가을)

## 먼저 읽기: SEAL 설치가 전제입니다

이 저장소에는 SEAL 라이브러리가 들어 있지 않습니다. `CMakeLists.txt` 는 **이 저장소의 상위 디렉토리에 있는 `seal-install/`** 에 SEAL 이 이미 설치되어 있다고 가정합니다.
그냥 clone 해서 `cmake` 하면 `find_package(SEAL)` 에서 실패합니다.

수업 서버에서 쓴 구성:

```
infosec/
├── SEAL/            # SEAL 소스 (v4.7.2)
├── seal-install/    # SEAL 설치 위치  <- CMakeLists.txt 가 찾는 곳
└── SEAL_TEST/       # 이 저장소 (수업 때는 학번 디렉토리마다 하나씩)
```

새 머신에서 처음부터 준비하려면:

```
git clone --branch v4.7.2 https://github.com/microsoft/SEAL.git
cd SEAL
cmake -S . -B build -DSEAL_USE_INTEL_HEXL=ON -DCMAKE_INSTALL_PREFIX=../seal-install
cmake --build build -j
cmake --install build
cd ..
git clone --branch lecture https://github.com/yonghaason/SEAL_TEST.git
cd SEAL_TEST
cmake -S . -B build
```

참고:
- 컴파일 플래그에 `-mavx512f -mavx512dq -mavx512vl` 이 들어 있습니다 (HEXL 용). AVX-512 가 없는 CPU 에서는 `CMakeLists.txt` 의 `CMAKE_CXX_FLAGS` 에서 이 플래그를 빼고, SEAL 도 `-DSEAL_USE_INTEL_HEXL=OFF` 로 빌드하세요.
- SEAL 을 다른 위치에 설치했다면 `cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/seal-install` 로 지정합니다.
- C++20 컴파일러가 필요합니다.

## 파일 구성

- `exec/` : 학생용 실습 파일 (빈칸 / TODO 상태). `common.h` 는 공용 출력 함수.
  - `sanitycheck.cpp` : 빌드 환경 확인용
  - `exp0_params.cpp` : 파라미터 설정, 암호화/복호화 (`___` 빈칸 채우기)
  - `exp1_noise.cpp` : 연산별 노이즈 예산 변화 (+ relinearize)
  - `exp2_modswitch.cpp` : modulus switching (BGV/BFV)
  - `exp3_lazy.cpp` : lazy relinearization
  - `exp4_rotation.cpp` : rotation 과 회전키 전략
  - `exp5_ckks.cpp` : CKKS 기본 (scale, rescale, 레벨 맞추기)
  - `exp6_logreg.cpp` : MNIST 숫자 인식 (10-클래스 로지스틱 회귀) 추론. `mnist_data.h` 는 모델과 테스트 이미지
  - `exp7_psi_basic.cpp` : 가장 단순한 PSI, r · ∏(y − x)
  - `exp8_psi_powers.cpp` : 거듭제곱을 더 보내기
  - `exp9_psi_bucket.cpp` : 해싱 + 파티션으로 큰 집합과 작은 집합의 PSI
  - `psi_hashing.cpp` : 해싱 시연 (채울 곳 없음)
  - `psi_common.h` : PSI 실험 공용 함수 (해싱, 다항식 계수)
- `solutions/` : 위 실험들의 정답 (학생 파일과 같은 주석 구조)
- `tools/make_mnist_data.py` : `exec/mnist_data.h` 를 만든 스크립트 (실습에는 필요 없음)
- `tools/logreg_plain.py` : 실험 6 의 로지스틱 회귀를 평문으로 계산 (W, b 는 `exec/mnist_data.h` 에서 읽음, numpy·pillow 필요). `python3 tools/logreg_plain.py images/builtin_7.png`, 직접 쓴 숫자는 `my.txt`
- `images/builtin_0.png` ~ `builtin_10.png` : 실험 6 의 내장 이미지 11 장 (MNIST 테스트 세트, 0 ~ 9 번은 숫자 0 ~ 9, 10 번은 모델이 틀리는 5). `tools/export_mnist_png.py` 로 만듦
- `tools/image_to_vector.py` : 손으로 쓴 숫자 이미지를 실험 6 입력 파일로 바꾸는 스크립트 (numpy, pillow 필요, 교수 시연용)
- `docs/plan.md` : 실습 전체 계획

## 빌드와 실행

처음 한 번만:

```
cmake -S . -B build
```

실험마다 따로 빌드하고 실행합니다 (아직 안 채운 실험이 있어도 다른 실험은 빌드됩니다).

```
cmake --build build --target sanitycheck     # 빌드 환경 확인: "x * y + x = 1.5 3 4.5 6" 이 나오면 정상
./build/sanitycheck

cmake --build build --target exp0_params
./build/exp0_params
```

정답으로 빌드하려면 (`solutions/` 에 있는 실험은 그 파일을 씁니다):

```
cmake -S . -B build-sol -DSOL=ON
cmake --build build-sol
```

## 실험 6 에 직접 쓴 숫자 넣기 (교수 시연용, 학생 디렉토리에는 배포하지 않음)

흰 바탕에 검은 글씨로 숫자 하나를 쓴 이미지 파일을 준비합니다.

```
python3 tools/image_to_vector.py my_digit.png -o my.txt
./build/exp6_logreg my.txt
```

인자 없이 실행하면 내장 이미지 0 번을 쓰고, `./build/exp6_logreg 7` 처럼 0 ~ 10 번을 고를 수 있습니다.
