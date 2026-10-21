# TraceMotive C++ 변수 후보 검색 성능 검증 — 2026-10-11

## 결과 한도 적용 후 재측정

출시 직전 수정 패키지(UE 5.8.3 Win64)를 사용해 `Saved/TraceMotiveReleaseReviewHost`의 `Source` fixture 528개를 재측정했다. 프로젝트 상위 경로에 `Saved`가 포함되어도 528개를 모두 큐에 넣었다. 기본 `MaxSearchResults=500`에 도달하여 후보 500개만 반환하고 소스 검색을 중단했으며, 제한 상태 회귀 검증이 통과했다. 큐 구성 5.193ms, 최대 틱 11.747ms, 전체 461.239ms, 137틱이었다. 종료 코드 0. 로그: `Project/ToyBuilderC/Saved/TraceMotiveReleaseReviewHost/FinalPerfRun.log`.

최대 틱은 여전히 명목 4ms 예산을 넘는다. 아래 528건 반환 수치는 결과 한도 적용 전 기준선이며, 수정 후 결과와 직접 비교할 때 한도 차이를 고려해야 한다.

## 환경과 에셋

- Unreal Engine 5.8.3, Windows 11, Win64 Development `BuildPlugin` 패키지, NullRHI 헤드리스 에디터.
- 별도 테스트 프로젝트 `TraceMotivePerfHost`의 `Source/TraceMotivePerfFixture`에 `.cpp` 파일 528개, 총 8,429,272바이트를 생성했다. 작은 파일 512개(약 8KiB), 큰 파일 16개(약 256KiB), 파일마다 `TMPerfValue` 식별자 후보 1개다.
- 재생성 도구: `Project/ToyBuilderC/Plugins/TraceMotive/Tools/Performance/GenerateVisualRefPerfFixture.ps1`. 예: `./GenerateVisualRefPerfFixture.ps1 -ProjectRoot <빈 UE 프로젝트 경로>`.
- 자동화: `TraceMotive.Performance.VisualReferenceCpp`. 소스 파일 열거, C++ 후보 콜백 및 실제 `VisualRefSearcher::Tick`을 측정한다. Blueprint Asset Registry 검색과 실제 Slate 결과 그래프 구성은 제외한다.
- 원본 실행 로그는 `Project/ToyBuilderC/Saved/TraceMotiveReleaseReviewHost/PerfRunConsole.log`, `PerfRun2.log`, `PerfRun3.log`에 보관했다. 측정에 사용한 임시 호스트는 로그 보존 후 제거했다. 같은 `Saved` 아래 보존된 에셋 사본은 스캐너의 제외 규칙 때문에 성능 실행 경로로 사용할 수 없다.

## 결과

| 실행 | 큐 파일 | 발견 후보 | 큐 생성 | 최대 틱 | 전체 검색 | 틱 수 | 결과 |
|---|---:|---:|---:|---:|---:|---:|---|
| 1 | 528 | 528 | 3.066ms | 10.216ms | 370.097ms | 144 | PASS |
| 2 | 528 | 528 | 2.957ms | 11.804ms | 374.577ms | 144 | PASS |
| 3 | 528 | 528 | 3.201ms | 11.329ms | 373.650ms | 144 | PASS |

**FACT:** 세 번 모두 파일과 후보 수가 일치했고 테스트 프로세스 종료 코드가 0이었다. 중앙값은 큐 3.066ms, 최대 틱 11.329ms, 전체 검색 373.650ms다.

**INFERENCE:** 이 에셋 집합에서 큐 생성 지연은 작지만, 최대 틱은 일반 검색 예산 4ms의 약 2.6~3.0배다. 한 파일의 로드·파싱은 분할되지 않으므로 더 큰 단일 소스 파일에서는 더 긴 프레임 지연이 가능하다. 실제 에디터 프레임 시간과 결과 UI 메모리는 측정하지 않았다.

## 범위와 후속 검수

- 이 수치는 로컬 소스 파일 검색 단계의 비교 기준이다. 고객 프로젝트의 저장 장치, 파일 크기 분포, 소스 수, 캐시 상태와 Slate 렌더링 비용에 따라 달라진다.
- 현재 후보 수 제한이 없으므로 동일 식별자가 매우 많은 프로젝트의 결과 UI 비용은 별도로 확인해야 한다.
- 출시 전 대표 대형 프로젝트에서 실제 에디터 검색 시작 지연, Unreal Insights 최대 틱, 결과 그래프 조작과 취소 응답성을 확인한다.
