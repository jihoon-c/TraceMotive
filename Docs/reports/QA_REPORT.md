# TraceMotive 참조 찾기 QA 보고서 (UE 5.8)

## 2026-10-19 최종 점검

최신 검증 범위·결과·출시 판단은 [출시 최종 점검 보고서](RELEASE_READINESS_2026-10-19.md)를 우선한다.
아래 내용은 이전 구현과 검증의 이력이며 현재 출시 승인으로 해석하지 않는다.
최종 DLL의 플러그인 자동화 16개·fixture 생성 2개, 빈 프로젝트 검색 회귀 5개가 통과했다.
콜드 BP 검색은 729.255ms에 완료됐고 제출 ZIP 146개 파일의 해시 불일치는 0개였다.
실제 UI/PIE 및 판매 페이지 지원 정보·미디어 권리는 여전히 확인이 필요하다.

## 2026-10-11 교차 Blueprint 전체 검색 검증

- **MAJOR / 발견 및 수정:** 기본 빠른 후보 검색은 메타데이터에 변수 이름이 없는 자식 BP를 큐에 넣지 않아 Project Content 결과에서 참조가 누락될 수 있었다. UI 기본값을 `Full scan`으로 바꾸고, 빠른 모드에는 누락 가능성 안내를 추가했다 (`TraceMotive.cpp:678`, `1245-1249`).
- **FACT / fixture:** 저장된 `BP_TM_VariableReferenceSample`의 자식 `BP_TM_VariableReferenceConsumer`를 생성했다. 자식 Event Graph에는 `TMBlueprintSampleValue`와 `TMReferenceSampleValue`의 GET/SET 노드가 각각 있다. 원본 프로젝트 에셋: `Project/ToyBuilderC/Content/TraceMotiveTests/Blueprints/BP_TM_VariableReferenceConsumer.uasset`.
- **FACT / PASS:** 최종 패키징 DLL의 `TraceMotive.ProjectFixture` 2개가 통과했다. Full scan 교차 BP 테스트에서 두 변수 모두 부모 BP 2건 + 자식 BP 2건을 찾았다. 종료 코드 0. 로그: `Project/ToyBuilderC/Saved/TraceMotiveBlueprintQAHost/FinalProjectFixtureBuild9.log`.
- **FACT / PASS:** 최종 UE 5.8.3 Win64 `BuildPlugin` 패키징 성공. 해당 DLL로 격리 호스트의 `TraceMotive.Search` 3개가 모두 통과했다(종료 코드 0). 로그: `Project/ToyBuilderC/Saved/TraceMotiveReleaseReviewHost/FinalSearchBuild9.log`.
- **MAJOR / 범위 최적화:** Project Content fallback은 `/Game`과 대상 BP 마운트로 제한했다. 격리 호스트에서 불필요한 엔진 플러그인 BP를 포함하던 fallback 261개가 프로젝트 범위 5개로 줄었다. 이는 검색 범위 수치이며 실제 고객 프로젝트 프레임 성능 수치는 아니다.
- **배포 주의:** 두 BP fixture와 그 생성용 ToyBuilderC 소스는 현재 `.gitignore`의 `Project/ToyBuilderC/*` 규칙으로 버전 관리에서 제외된다. 로컬 프로젝트에는 저장돼 있지만 플러그인 패키지나 저장소 복제본에는 포함되지 않는다.
- **잔여:** 빈 에디터에서 UI를 클릭한 수동 탐색·결과 점프, 대형 고객 프로젝트, 엔진·외부 플러그인 콘텐츠 참조의 완전성은 검증하지 않았다. 아래 이전 결과는 당시의 구현 상태를 설명한다.

## 2026-10-11 저장된 Blueprint fixture 종단 검증

- **FACT / PASS:** `/Game/TraceMotiveTests/Blueprints/BP_TM_VariableReferenceSample`을 격리 `ToyBuilderC` 프로젝트에서 로드했다. 부모는 `ATraceMotiveFeatureLabActor`다.
- **FACT / PASS:** `TraceMotive.ProjectFixture.BlueprintVariableReferences`가 실제 저장 BP의 `TMBlueprintSampleValue`와 부모 C++ 프로퍼티 `TMReferenceSampleValue` 각각에서 GET 1개, SET 1개를 찾았다. `Current Blueprint` 범위 검색으로 실행했고 종료 코드 0이었다. 로그: `Project/ToyBuilderC/Saved/TraceMotiveBlueprintQAHost/BlueprintReferenceRun.log`.
- **범위:** 이 테스트는 동일 BP 내부의 변수 참조 4개를 확인한다. 다른 BP의 참조 발견은 위 교차 BP 테스트로 별도 확인했다. 함수·디스패처·인터페이스·매크로·상속 전체 조합과 실제 UI 원본 이동의 완전성은 아직 입증하지 않는다.

## 2026-10-11 출시 직전 기능 수정 검증

**릴리스 권고: 실제 에디터 사용 흐름 검수 전까지 조건부 보류.** 아래 자동 검증은 통과했으나 UI, PIE와 고객 규모 프로젝트 성능은 미검증이다. 이전 HOLD 기록은 수정 전 상태를 설명한다.

| 심각도 | 항목 | 상태와 근거 |
|---|---|---|
| MAJOR | 재검색·창 종료 후 오래된 검색 결과가 새 그래프에 섞이거나 해제된 창을 갱신할 위험 | **FIXED / FACT (코드 경로 및 빌드):** 활성 검색을 취소한 뒤 재검색하며, 창 종료 시 취소한다. 실제 UI 반복 클릭은 미실행. |
| MAJOR | C++ 후보가 무제한으로 결과 그래프에 누적됨 | **FIXED / FACT:** 설정 결과 한도 적용 및 화면 제한 안내. 528개 파일 fixture에서 500건 반환, 조기 종료 상태 테스트 PASS. |
| MAJOR | 프로젝트 상위 경로의 `Saved` 때문에 C++ 소스 전체가 제외됨 | **FIXED / FACT:** 프로젝트 상대 경로로 제외 판정. `Saved/TraceMotiveReleaseReviewHost`의 `Source` fixture 528개가 모두 큐에 들어감. |
| MAJOR | BP 재컴파일·클래스 언로드 뒤 변수 추적이 오래된 클래스 포인터를 보유함 | **FIXED / FACT (코드 경로 및 빌드):** 약한 클래스 참조와 유효성·교체 상태 확인 후 추적 중단. 실제 PIE 재컴파일 재현은 미실행. |
| MAJOR | C++ 단일 파일 처리 중 프레임 지연 | **OPEN / FACT:** 528개 fixture에서 최대 틱 11.747ms, 명목 예산 4ms 초과. 실제 Slate 프레임 체감은 미측정. |

**자동 검증:** UE 5.8.3 Win64 `BuildPlugin` PASS; 패키징 DLL 격리 호스트의 `TraceMotive.Search` 3개 및 `TraceMotive.Collision.SettingsAndResume` 1개 PASS, 모두 종료 코드 0. 성능 fixture는 큐 528개, 후보 500개(결과 한도), 큐 구성 5.193ms, 최대 틱 11.747ms, 전체 461.239ms, 137틱으로 PASS. 로그: `Project/ToyBuilderC/Saved/TraceMotiveReleaseReviewHost/FinalSearchRun.log`, `FinalCollisionRun.log`, `FinalPerfRun.log`.

**미검증:** 실제 에디터의 네 기능 클릭·결과 이동·재검색·창 종료, 멀티 PIE, 변수 추적 중 BP 재컴파일, 대규모 고객 프로젝트 Slate/프레임 성능, 지원을 주장할 다른 UE 버전.

## 2026-10-11 재검수 결과

2026-10-10의 범위·설명 문제는 수정됐다. UE 5.8.3 Win64 `BuildPlugin` 성공, 패키징 DLL을 넣은 격리 호스트에서 `TraceMotive.Search` 3개 테스트 성공(종료 코드 0). `Current Blueprint` 및 `Same Folder`에서 C++ 큐가 비어 있음을 새 회귀 검증이 확인했다. `Project Content` 성능 테스트는 별도 호스트의 소스 528개를 모두 큐에 넣고 후보 528건을 발견했으며 3회 모두 성공했다.

**릴리스 판단:** 요청한 수정·자동 검증은 완료됐다. 다만 최대 C++ 검색 틱이 10.2~11.8ms로 명목 4ms 예산을 넘었고, 실제 UI/PIE 및 대형 고객 프로젝트 검수는 미실시다. 출시 승인 판단에는 이 잔여 범위를 별도로 확인해야 한다. 아래 이전 HOLD 항목은 수정 전 기록이다.

---

## 2026-10-10 출시 후보 추가 검수

**릴리스 권고: HOLD.** 현재 작업 트리는 UE 5.8.3 Win64 패키징과 검색 자동화 3개를 통과했다. 아래 범위·상품 설명 문제를 해결하고 핵심 기능의 실제 에디터 사용 흐름을 확인한 뒤 제출한다. 검수 환경은 Windows 11, UE 5.8.3, 패키징된 플러그인을 넣은 빈 테스트 프로젝트, 헤드리스 NullRHI 에디터다.

### MAJOR — 선택한 검색 범위 밖의 C++ 결과가 포함됨

- **사전 조건/절차:** C++ 파일에 대상 변수와 동명 식별자를 두고 Visual Find References에서 `Current Blueprint` 또는 `Same Folder`를 선택한다.
- **기대:** 선택 범위의 결과만 표시하거나 C++ 범위가 별개임을 명시한다.
- **실제:** **FACT (코드 경로 검증)** C++ 큐는 두 선택과 무관하게 프로젝트 `Source/Plugins` 전체 파일로 구성된다 (`VisualRefSearcher.cpp:1492-1542`, `1659-1671`). 실제 UI 재현은 이번 검수에서 수행하지 않았다.
- **빈도/영향:** 모든 변수 검색에서 해당 코드 경로를 탄다. 범위 신뢰와 대형 프로젝트 응답성에 영향. 출시 전 범위별 종단 재현 필요.

### MAJOR — 고객에게 보이는 기능 설명과 v1 노출 정책이 다름

- **사전 조건/절차:** 플러그인 정보의 Description을 읽고 input failure, packaging progress, development workflow 기능을 찾는다.
- **기대:** 설명의 기능을 사용할 수 있다.
- **실제:** **FACT (구성 검증)** `TRACEMOTIVE_CORE_ONLY_RELEASE=1`은 네 핵심 도구만 노출하지만 `TraceMotive.uplugin` Description은 비노출 기능까지 설명한다.
- **빈도/영향:** 설치 정보를 읽는 고객에게 항상 나타나는 불일치. 제출 전 문구 수정 필요.

### 통과 및 미검증

- **PASS:** UE 5.8.3 Win64 BuildPlugin 패키징 성공.
- **PASS:** `TraceMotive.Search.AssetUsageEvidenceAndResume`, `CallChainPackageResumesAfterBudget`, `VisualReferenceResumesAfterBudget` 3개 모두 성공. 로그: `Project/ToyBuilderC/Saved/TraceMotiveReleaseReviewHost/Saved/Logs/TraceMotiveReleaseReviewHost.log:2033-2051`.
- **NOT RUN:** 실제 에디터에서 네 핵심 도구 클릭, C++ 파일·라인 열기, PIE 변수 샘플링, 충돌 쌍 분석, 멀티 PIE, 취소·재검색, 대형 프로젝트 프레임 시간, 다른 엔진 버전 및 다른 운영체제. 자동화 3개 통과는 이 항목의 통과 증거가 아니다.

---

> 상태 업데이트: 아래 상세 항목은 C++ 변수 참조 후보 검색 추가 전 기준선이다. 이후 변수 검색에 Project `Source`/`Plugins` C++ 식별자 후보와 파일·라인 이동이 추가되었으며, 최종 UE 5.8 `BuildPlugin`과 `TraceMotive.Search` 3개 테스트가 통과했다. 동일 이름 멤버를 의미적으로 구분하는 C++ 심볼 인덱서는 아직 아니다.

## 릴리스 판단

**블루프린트 참조 + C++ 변수 식별자 후보 기능으로는 GO이며, “모든 C++ 및 블루프린트 참조를 의미 기반으로 정확히 찾는다”는 주장 기준으로는 여전히 NO-GO이다.**

플러그인 자체의 UE 5.8 빌드와 현재 제공되는 검색 자동화 테스트는 통과했다. 다만 통과한 테스트는 제한된 블루프린트 검색 및 시간 분할 동작을 검증할 뿐, C++/블루프린트 전체 참조의 완전성과 정확성을 검증하지 않는다. UI와 문서를 `Blueprint exact graph references` 및 `C++ source text candidates`로 제한하면 조건부 출시가 가능하다.

## 테스트 환경

- 엔진: Unreal Engine 5.8 (`F:\UE_5.8`)
- 플랫폼: Windows 64-bit
- 대상: `Project/ToyBuilderC/Plugins/TraceMotive`
- 빌드: `RunUAT BuildPlugin`, Win64, Rocket 모드
- 자동화: 패키징된 플러그인을 빈 임시 프로젝트에 설치 후 `UnrealEditor-Cmd.exe -NullRHI`
- 실행 필터: `TraceMotive.Search`

## 결과 요약

| 항목 | 결과 | 근거 |
|---|---:|---|
| UE 5.8 Win64 플러그인 패키징 | PASS | `BUILD SUCCESSFUL` |
| Asset usage evidence/resume | PASS | `TraceMotive.Search.AssetUsageEvidenceAndResume` |
| Call-chain package resume | PASS | `TraceMotive.Search.CallChainPackageResumesAfterBudget` |
| Visual reference resume/basic variable refs | PASS | `TraceMotive.Search.VisualReferenceResumesAfterBudget` |
| 모든 Blueprint 함수/변수/디스패처 참조 완전성 | NOT PROVEN | 대응 종단 fixture 부재 |
| 모든 C++ 참조 완전성 | FAIL BY DESIGN | 함수 텍스트 후보 외 C++ 검색 없음 |
| C++ 함수 후보의 오탐·누락 없음 | NOT PROVEN | 의미 기반 분석 및 정확도 테스트 부재 |

## 발견 사항

### MAJOR — C++ 참조 전체 검출 요구를 충족하지 못함

- **사전 조건:** C++ 코드에 프로퍼티 접근, 델리게이트 사용, 자산 경로 참조 또는 함수 호출이 존재한다.
- **절차:** 해당 대상에 대해 TraceMotive 참조 찾기/자산 사용처/호출 체인을 실행한다.
- **기대:** C++와 블루프린트의 관련 참조가 종류와 무관하게 모두 표시된다.
- **실제:** 블루프린트 그래프 검색이 중심이며, C++는 함수 호출 체인의 텍스트 후보에만 제한된다. C++ 변수·델리게이트·자산 사용처는 검색하지 않는다.
- **증거:** `VisualRefSearcher.cpp:980`, `FunctionCallChainTracer.cpp:1556-1558`, `TMAssetUsageLocator.cpp:1171-1187`.
- **빈도:** 해당 C++ 참조 유형마다 재현 가능.
- **신뢰도:** 높음(구현 경로 직접 확인).
- **영향:** 참조가 없다고 오판하고 코드/자산을 변경 또는 삭제할 위험.

### MAJOR — C++ 함수 결과는 오탐·누락 가능성이 있는 텍스트 후보

- **사전 조건:** 동일 이름 오버로드/타 클래스 함수, 매크로 호출, 여러 줄 호출 중 하나가 존재한다.
- **절차:** 함수 호출 체인에서 C++ 소스 결과를 확인한다.
- **기대:** 대상 심볼의 실제 호출만 표시되고 모든 호출이 포함된다.
- **실제:** 한 줄 텍스트 휴리스틱으로 후보를 만들며 구현도 수동 검증을 요구한다.
- **증거:** `FunctionCallChainTracer.cpp:741`, `FunctionCallChainTracer.cpp:2334`, `FunctionCallChainTracer.cpp:2407`.
- **빈도:** 코드 스타일 및 동일 이름 심볼 수에 따라 달라짐.
- **신뢰도:** 높음(알고리즘 직접 확인), 실제 프로젝트 오탐률은 미측정.
- **영향:** 호출 체인의 신뢰성이 낮아지고 수동 검증 비용 발생.

### MINOR — 검색 루트와 결과 수 제한

- **사전 조건:** 참조가 엔진 소스/프로젝트 외부 경로에 있거나 후보가 300건을 초과한다.
- **기대:** “모든 참조” 범위라면 해당 참조도 검색된다.
- **실제:** `Project/Source`, `Project/Plugins`만 스캔하며 C++ 후보는 300건에서 제한된다.
- **증거:** `FunctionCallChainTracer.cpp:50`, `FunctionCallChainTracer.cpp:2255-2256`, `FunctionCallChainTracer.cpp:2384`.
- **빈도:** 대형 프로젝트 또는 공용 함수에서 증가.
- **신뢰도:** 높음.
- **영향:** 결과 집합이 조용히 불완전해질 수 있음.

## 통과한 검증의 의미

- 자산 사용처 테스트는 객체/핀/기본 프로퍼티 증거와 시간 분할 재개를 검증한다.
- Visual Reference 테스트는 중첩 그래프의 변수 read/write 2건, 무관한 BP의 동일 이름 제외, 중복 방지를 검증한다.
- Call Chain 테스트는 만료된 시간 예산 후 패키지 스캔이 재개되고 다음 상태로 전이되는지만 검증한다.
- 위 결과는 함수·디스패처·네이티브 경계 및 C++ 텍스트 검색 정확도의 통과 증거가 아니다.

## 미검증 항목

- BP 함수 호출, 커스텀 이벤트, 오버라이드, 인터페이스 구현의 기대 참조 집합
- 이벤트 디스패처 bind/unbind/call/event 노드 전 조합
- 부모 C++ 클래스의 `UFUNCTION`/`UPROPERTY`를 사용하는 여러 BP 간 검색
- BP macro library 및 중첩 macro graph
- C++ 오버로드, 네임스페이스, 동일 이름 타 클래스 메서드
- 여러 줄 호출, 매크로, 함수 포인터, 델리게이트, 리플렉션 호출
- C++ 자산 hard/soft reference와 생성자 참조
- 언로드된 실제 프로젝트 BP 패키지의 비동기 검색
- 결과 더블클릭 후 에디터 그래프/노드 이동
- 후보 300건 초과 및 대형 프로젝트 성능

## 권장 회귀 테스트 세트

1. 최소 fixture 모듈에 C++ 클래스 2개, 오버로드, 동일 이름 함수, 멀티라인 호출, 매크로 호출을 둔다.
2. 부모 C++ 클래스와 자식 BP 2개, BP 인터페이스, 디스패처, 매크로 라이브러리를 구성한다.
3. 참조 유형별 기대 파일/라인 또는 BP 노드 GUID 집합을 고정한다.
4. 발견된 집합과 기대 집합을 양방향 비교해 누락과 오탐을 함께 실패 처리한다.
5. C++ 텍스트 후보는 정확 참조와 별도 테스트·표시한다.

## 최종 권고

- 포괄적 참조 검색 기능으로 출시하거나 홍보하지 않는다.
- 현 상태로 제공할 경우 기능명을 명확히 제한하고 C++ 결과에 `텍스트 후보—수동 검증 필요`를 노출한다.
- “모든 참조”가 제품 요구사항이라면 의미 기반 C++ 인덱싱과 위 회귀 fixture를 완료한 뒤 재검증한다.
