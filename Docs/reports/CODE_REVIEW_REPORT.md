# TraceMotive 참조 찾기 코드 검토 보고서 (UE 5.8)

## 2026-10-19 최종 점검

최신 판정과 증거는 [출시 최종 점검 보고서](RELEASE_READINESS_2026-10-19.md)를 우선한다.
아래 내용은 이전 검토의 이력이다. 이번에는 Current Blueprint의 시간 분할
완료 처리, 클래스·GUID 소유자 식별, 로컬 변수 범위, CustomEvent 분기,
함수 진입 메뉴와 네이티브 형제 BP 참조 판정을 수정하고 회귀를 추가했다.
기본 검색의 C++ 자동 순회가 콜드 테스트 완료를 30초 넘게 지연시키는 것도 확인해
C++ 텍스트 후보를 기본 꺼짐인 명시적 선택 옵션으로 전환했다. 최종 자동화와
제출 소스 ZIP의 코드 동일성 검증은 통과했다. 상세 증거는 위 최신 보고서에 있다.

## 2026-10-11 교차 Blueprint 누락 수정

- **FACT:** 기존 Project Content 빠른 경로는 메타데이터/역참조 우선 후보만 검색해, 열려 있지 않은 자식 BP의 `TMBlueprintSampleValue` 참조를 놓쳤다. 저장된 부모·자식 BP fixture로 재현했다.
- **수정:** UI의 `Full scan`을 기본값으로 설정했다. 빠른 모드를 사용자가 선택하면 누락 가능성을 툴팁에 표시한다. 전체 검색의 fallback은 `/Game`과 대상 BP 콘텐츠 마운트로 제한했다.
- **검증:** `TraceMotive.ProjectFixture.CrossBlueprintVariableReferences`가 두 변수 각각에서 부모 BP 2건·자식 BP 2건을 찾았다. 격리 호스트의 대상 큐는 무관한 엔진 플러그인 BP 262개에서 프로젝트 범위 6개로 줄었다. 실제 대형 프로젝트 성능은 별도 검수 대상이다.

## 2026-10-11 출시 직전 기능 수정 재검토

- **FACT / 수정:** 재검색 시 이전 `VisualRefSearcher`를 취소하고, 참조 창을 닫을 때도 활성 검색을 취소한다 (`TraceMotive.cpp:855-862`, `1469-1474`). C++ 결과 콜백은 에디터가 살아 있을 때만 그래프를 갱신한다.
- **FACT / 수정:** C++ 변수 후보에 설정된 `MaxSearchResults` 한도를 적용하고, 한도 도달 시 소스 검색을 중단하며 화면에 결과 제한 안내를 표시한다 (`VisualRefSearcher.cpp:1563-1588`, `TraceMotive.cpp:1150-1164`). 후보 파서도 단일 파일 내 결과 수를 제한한다.
- **FACT / 수정:** C++ 스캔 제외 폴더를 프로젝트 상대 경로로 판정한다. 프로젝트의 상위 경로에 `Saved`가 있어도 실제 프로젝트 `Source`가 검색된다 (`VisualRefSearcher.cpp:1533-1542`).
- **FACT / 수정:** 변수 값 추적의 대상 클래스를 약한 참조로 보관하고, 클래스 교체 또는 언로드 감지 시 추적을 중단하고 다시 선택하도록 안내한다 (`TMVariableValueTrace.cpp:279-290`, `886`).
- **FACT / 검증:** UE 5.8.3 Win64 `BuildPlugin` 성공. 패키징 DLL을 사용한 `TraceMotive.Search` 3개와 `TraceMotive.Collision.SettingsAndResume` 1개가 통과했다. 528개 C++ 파일 fixture는 528개를 큐에 넣고 기본 결과 한도 500건을 반환했으며, 제한 상태 테스트가 통과했다.
- **남은 위험 / INFERENCE:** 파일 열거와 개별 파일 로드·파싱은 동기 작업이다. fixture 최대 틱 11.747ms로 명목 4ms 예산을 넘었다. 실제 Slate 결과 UI, PIE 클래스 재컴파일, 여러 UE 버전의 에디터 흐름은 아직 수동 검증이 필요하다. 아래 이전 발견 사항과 판단은 해당 수정 전 이력이다.

## 2026-10-11 수정 후 상태

아래 2026-10-10의 검색 범위 및 플러그인 설명 발견 사항은 **수정 완료**됐다. `Current Blueprint`와 `Same Folder`는 Blueprint 그래프만 검색하고, `Project Content`에서만 프로젝트 C++ 변수 식별자 후보를 스캔한다 (`VisualRefSearcher.cpp:1492-1497`). descriptor와 가이드·README·UI 툴팁도 같은 범위를 설명한다. UE 5.8.3 Win64 패키징 및 `TraceMotive.Search` 자동화 테스트 3개가 통과했다.

성능 위험은 **검증 후에도 남는다**. 528개 C++ 테스트 파일(8,429,272바이트)에서 큐 구성 2.96~3.20ms, 전체 소스 검색 370.10~374.58ms, 최대 틱 10.22~11.80ms였다. 일반 검색의 명목 틱 예산은 4ms (`TMPerformanceGuard.h:196-200`)지만 한 파일의 로드·파싱은 중단하지 않는다. 결과 수 제한과 실제 Slate 결과 그래프 비용도 이 테스트에서는 측정되지 않았다. 상세 조건은 `TRACE_MOTIVE_PERFORMANCE_REPORT_2026-10-11.md`를 참조한다.

---

## 2026-10-10 출시 후보 추가 검토

**판단: 현재 작업 트리 그대로의 Fab v1 출시는 보류 권고.** 아래 첫 두 항목을 고치고 범위별 실제 에디터 검수를 마친 뒤 재판단한다. 이번 검토는 플러그인의 미커밋 변경 13개 파일과 관련 테스트·출시 문서에 한정했다. 엔진 소스는 열지 않았다. 프로덕션 코드는 수정하지 않았다.

### HIGH — 범위 선택이 C++ 검색에 적용되지 않음

- **FACT / 증거:** `VisualRefSearcher::StartSearch`는 변수 검색이면 범위와 무관하게 `BuildCppSourceFileQueue`를 호출한다 (`VisualRefSearcher.cpp:1492-1495`). 큐는 `Project/Source`와 `Project/Plugins` 전체를 수집한다 (`VisualRefSearcher.cpp:1513-1542`). `CurrentBlueprint` 분기도 이 큐가 남아 있으면 계속 검색한다 (`VisualRefSearcher.cpp:1659-1671`). `SameFolder` 제한은 Blueprint 자산 큐에만 적용된다 (`VisualRefSearcher.cpp:1088-1109`).
- **영향 / 재현:** 변수로 Visual Find References를 열고 `Current Blueprint` 또는 `Same Folder`를 선택하면, 선택 범위 밖 C++ 파일의 동명 식별자 후보까지 결과에 표시되고 전체 C++ 파일 스캔 비용을 지불한다. 사용자가 표시된 범위를 결과의 실제 범위로 오해한다.
- **권고:** C++ 검색을 별도 명시적 범위로 표시하거나 선택 범위에 맞게 큐를 제한한다. 세 범위 각각에 대해 실제 파일 집합과 UI 결과를 검증한다.

### HIGH — 출시 플러그인 설명이 비노출 기능을 광고함

- **FACT / 증거:** `TraceMotive.Build.cs`는 `TRACEMOTIVE_CORE_ONLY_RELEASE=1`이고 `TraceMotive.cpp:128-141`은 다수 기능 메뉴를 등록하지 않는다. 그러나 `TraceMotive.uplugin`의 Description은 input failures, packaging progress, development workflows까지 제품 기능으로 설명한다. `CORE_RELEASE_QA.md`는 네 핵심 기능만 v1에 등록한다고 명시한다.
- **영향 / 재현:** 설치된 플러그인 정보에서 설명을 읽고 해당 도구를 찾으면 실행 진입점이 없다. Fab 상품 설명으로 재사용하면 실제 제공 기능과 어긋난다.
- **권고:** descriptor Description과 제출 페이지를 네 핵심 기능 및 실제 C++ 후보 검색 범위에 맞춰 수정한다.

### MEDIUM — C++ 검색의 시작 작업과 결과 수가 제한되지 않음

- **FACT / 증거:** `BuildCppSourceFileQueue`의 재귀 파일 열거·정렬은 UI가 호출하는 `StartSearch` 안에서 동기 실행된다 (`VisualRefSearcher.cpp:1492-1495`, `1507-1544`). `ScanNextCppSourceFile`은 한 파일의 전체 로드·파싱과 모든 식별자 후보 콜백을 한 틱에서 실행한다 (`1552-1573`). 후보 콜백은 결과를 제한 없이 `TargetNode->References`에 추가한다 (`TraceMotive.cpp:1011-1047`).
- **INFERENCE / 영향:** 큰 소스 트리나 동일 이름 식별자가 많은 코드에서 검색 시작·단일 틱 정지와 대형 결과 그래프가 생길 수 있다. 정량 프레임 시간은 측정하지 않았다.
- **권고:** 파일 열거를 분할하거나 비동기화하고, 후보 수·소스 크기 제한과 잘림 표시를 추가한다. 대표 대형 프로젝트에서 시작 지연·최대 틱 시간을 측정한다.

### 확인 및 남은 검증

- **FACT:** UE 5.8.3 Win64 `RunUAT BuildPlugin -Rocket` 성공 (`BUILD SUCCESSFUL`, 2026-10-10).
- **FACT:** 패키징된 플러그인을 빈 호스트에서 실행한 `TraceMotive.Search` 자동화 테스트 3개 모두 `Success`, 종료 코드 0 (`Saved/TraceMotiveReleaseReviewHost/Saved/Logs/TraceMotiveReleaseReviewHost.log:2033-2051`).
- **한계:** C++ 파일 탐색의 범위별 종단 테스트, 실제 UI 클릭·파일 이동, 대형 프로젝트 성능, PIE 다중 인스턴스, 충돌 분석 실제 흐름은 이번 실행에서 검증하지 않았다.

---

> 상태 업데이트: 이 보고서의 상세 발견 사항은 C++ 변수 참조 후보 검색을 추가하기 전 기준선이다. 이후 변수 검색에 Project `Source`/`Plugins`의 C++ 정확 식별자 후보, READ/SET 분류, 파일·라인 이동이 추가되었고 UE 5.8 빌드 및 검색 자동화 테스트를 통과했다. 의미 기반 C++ 심볼 분석이 아니므로 동일 이름 멤버·매크로·리플렉션 접근에 관한 한계는 유지된다.

## 결론

**현재 구현은 블루프린트 참조와 C++ 변수 식별자 후보를 함께 제공하지만, “모든 C++와 블루프린트 참조를 의미 기반으로 정확히 찾는다”고 판단할 수는 없다.**

- **FACT:** 블루프린트 변수·함수·디스패처 검색은 `UBlueprint` 그래프의 K2 노드를 대상으로 한다 (`VisualRefSearcher.cpp:980`, `VisualRefSearcher.cpp:1052-1068`). 네이티브 `UFUNCTION`/리플렉션 프로퍼티를 호출하는 **블루프린트 노드**는 대상이 될 수 있지만 `.cpp/.h` 참조를 검색하는 기능은 아니다.
- **FACT:** C++ 소스 검색은 함수 호출 체인의 부가 단계에만 있으며, 한 줄 텍스트에서 `FunctionName(` 형태를 추정하는 휴리스틱이다 (`FunctionCallChainTracer.cpp:741`, `FunctionCallChainTracer.cpp:2334`).
- **FACT:** 자산 사용처 검색은 메뉴명부터 `Find Exact Blueprint Usage`이고, 후보도 블루프린트 자산만 적재한다 (`TMAssetUsageLocator.cpp:1171-1187`, `TMAssetUsageLocator.cpp:1745`).
- **INFERENCE:** 따라서 C++ 변수, C++ 디스패처/델리게이트, C++ 자산 참조 및 C++ 함수의 모든 의미론적 참조를 완전하게 검출한다는 제품 주장은 성립하지 않는다.

## 검토 범위

- `Project/ToyBuilderC/Plugins/TraceMotive` 내부의 참조 검색 구현과 자동화 테스트
- UE 5.8 `BuildPlugin` Win64 패키징
- UE 5.8 빈 호스트에서 `TraceMotive.Search` 자동화 테스트
- 엔진 소스는 플러그인 코드만으로 동작과 한계를 판단할 수 있어 확인하지 않음
- 이번 검토에서는 프로덕션 코드를 수정하지 않음

## 주요 발견 사항

### HIGH — 전체 C++/블루프린트 참조 검색 기능이 아님

**증거**

- 변수·함수·디스패처의 Visual Find References 진입점은 블루프린트와 그래프 노드를 컨텍스트로 사용한다 (`TraceMotive.cpp:214-286`, `TraceMotive.cpp:290-380`).
- 실제 검색 대상은 `UBlueprint`이며 변수는 `UK2Node_Variable`, 함수는 `UK2Node_CallFunction`, 디스패처는 K2 이벤트/델리게이트 노드를 판정한다 (`VisualRefSearcher.cpp:980`, `VisualRefSearcher.cpp:1645`, `VisualRefSearcher.cpp:1747`, `VisualRefSearcher.cpp:2111`).
- C++ 소스 큐는 함수 호출 체인에서만 구성된다 (`FunctionCallChainTracer.cpp:1556-1558`).
- 자산 검색은 블루프린트 자산만 후보로 추가한다 (`TMAssetUsageLocator.cpp:1171-1187`).

**영향**

- 사용자가 C++ 변수 읽기/쓰기, 델리게이트 바인딩/브로드캐스트, 생성자나 런타임 로드의 자산 경로를 기대하면 누락된다.
- UI나 문서에서 범위를 “모든 참조”로 표현하면 실제 동작과 불일치한다.

**재현 조건**

1. C++ 클래스에서 다른 C++ 클래스의 프로퍼티를 직접 읽거나 쓴다.
2. C++에서 델리게이트를 바인딩하거나 브로드캐스트한다.
3. C++에서 `ConstructorHelpers`, `LoadObject`, `TSoftObjectPtr` 경로로 자산을 참조한다.
4. 각각을 현재 Visual Find References 또는 Exact Blueprint Usage로 검색한다.

**권고**

- 현재 UI/문서는 “Blueprint graph references”와 “C++ source text candidates”로 범위를 명시한다.
- 완전한 C++ 지원이 목표라면 UnrealHeaderTool/리플렉션 정보만으로는 부족하므로 Clang 기반 인덱스 또는 IDE/컴파일 데이터베이스 연동을 별도 기능으로 설계한다.

### HIGH — C++ 함수 검색은 정확한 심볼 참조가 아니라 텍스트 후보 검색

**증거**

- `LooksLikeCppCallSite`는 한 줄 문자열에서 함수명과 괄호 형태를 검사한다 (`FunctionCallChainTracer.cpp:741`).
- 결과 설명도 `Text scan result; verify overloads/macros manually`라고 명시한다 (`FunctionCallChainTracer.cpp:2407`).
- 최대 300건에서 결과가 잘린다 (`FunctionCallChainTracer.cpp:50`, `FunctionCallChainTracer.cpp:2384`).

**영향**

- 같은 이름의 오버로드나 다른 클래스 메서드를 혼동할 수 있다.
- 여러 줄 호출, 매크로 간접 호출, 함수 포인터·델리게이트, 리플렉션 기반 호출은 누락될 수 있다.
- 문자열/전처리문/유사 문법은 오탐 후보가 될 수 있다.

**권고**

- 결과를 “정확한 참조”와 같은 목록에 섞지 말고 `C++ text candidate`로 시각적으로 분리한다.
- 파일·라인뿐 아니라 신뢰도와 검증 필요 표시를 유지한다.
- 오버로드, 동일 이름 타 클래스, 여러 줄 호출, 매크로, 주석·문자열에 대한 양성/음성 고정 테스트를 추가한다.

### MEDIUM — C++ 검색 범위가 프로젝트 Source/Plugins로 제한됨

**증거**

- 검색 루트는 `ProjectDir/Source`와 `ProjectDir/Plugins`뿐이다 (`FunctionCallChainTracer.cpp:2255-2256`).
- 엔진 소스 루트는 추가되지 않는다.

**영향**

- 엔진 내부 호출자, 외부 플러그인 경로, 프로젝트 루트 밖의 모듈 소스는 결과에서 빠진다.

**권고**

- UI에 검색 루트를 표시하고 사용자가 명시적으로 추가할 수 있게 한다.
- 엔진 소스 포함은 비용이 크므로 기본 비활성 옵션으로 제공하고 성능 예산을 둔다.

### MEDIUM — 자동화 테스트가 완전성·정확성을 검증하지 않음

**증거**

- `VisualReferenceResumesAfterBudget`는 시간 분할 재개, 중첩 그래프의 변수 read/write, 무관한 동일 이름 BP 제외, 중복 방지만 검증한다 (`TMTraceSessionTests.cpp:50-98`).
- `CallChainPackageResumesAfterBudget`는 패키지 스캔 재개와 상태 전이만 검증한다 (`TMTraceSessionTests.cpp:100-124`).
- C++ 소스 후보 검색의 실제 양성·음성 테스트가 없다.
- 함수, 디스패처, 인터페이스, 상속, 네이티브 멤버, 매크로 내부 참조에 대한 종단 테스트가 없다.

**영향**

- 현재 3개 테스트 통과는 “모든 참조가 정확하다”는 증거가 아니다.

**권고**

- UE 5.8용 고정 fixture 프로젝트를 추가해 BP↔BP, BP→C++, C++→BP, C++↔C++ 경계를 각각 검증한다.
- 각 fixture에 기대 참조 집합을 선언하고 누락과 오탐을 동시에 검사한다.

## 확인된 정상 동작

- UE 5.8 Win64 `BuildPlugin` 패키징 성공.
- UE 5.8 격리 호스트에서 다음 3개 테스트 성공:
  - `TraceMotive.Search.AssetUsageEvidenceAndResume`
  - `TraceMotive.Search.CallChainPackageResumesAfterBudget`
  - `TraceMotive.Search.VisualReferenceResumesAfterBudget`
- 블루프린트 변수 검색의 시간 분할 재개 및 기본 read/write 판정은 자동화 테스트로 확인됨.

## 불확실성 및 테스트 공백

- **ASSUMPTION:** 실제 대형 프로젝트의 에셋 레지스트리 메타데이터와 비동기 로딩 상태는 격리 fixture와 다를 수 있다.
- 에디터 UI에서 결과 클릭 후 정확한 그래프/노드 이동은 이번 헤드리스 실행에서 확인하지 않았다.
- 블루프린트 인터페이스, 이벤트 오버라이드, 매크로 라이브러리, 상속 계층의 동일 이름 멤버는 전용 fixture로 검증되지 않았다.
- C++ 소스 휴리스틱의 recall/precision 수치는 측정되지 않았다.
