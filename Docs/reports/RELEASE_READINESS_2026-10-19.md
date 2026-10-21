# TraceMotive 1.0.0 출시 최종 점검 — 2026-10-19

## 판정

**기술 검증 통과, 제출 판단은 조건부 보류.** 코드에서 확인한 참조 누락·오탐
경로를 수정했고 최종 DLL의 플러그인 자동화 16개와 fixture 생성 2개,
별도 빈 프로젝트의 검색 회귀 5개가 모두 통과했다. 제출용 소스 ZIP도 생성·검증했다.
실제 에디터 조작·PIE 흐름과 판매 페이지의 지원 정보·미디어 권리 확인은
현재 증거로 완료 판정할 수 없다. 이전 보고서의 GO/HOLD 문구보다 이 보고서를 우선한다.

## 범위와 환경

- FACT: `Project/ToyBuilderC/Plugins/TraceMotive`의 참조 검색 구현,
  관련 UI 진입점, 테스트, 플러그인 descriptor와 고객 문서를 점검했다.
- FACT: Windows, UE 5.8.3, 엔진/모듈 Build ID `55116800`.
  다른 UE 버전·운영체제 지원은 이번 검증에 포함하지 않는다.
- FACT: 프로젝트 코드가 없는 격리 호스트의 빌드와 검색 회귀,
  별도 ToyBuilderC QA 호스트의 저장 BP fixture를 사용한다.
- FACT: 엔진 소스 확인은 `FMemberReference`, BP 노드, 플러그인 API와
  My Blueprint 메뉴의 필요한 선언·구현 부분으로 제한했다.

## 발견 및 수정

| 심각도 | 확인한 문제와 영향 | 수정과 근거 |
|---|---|---|
| MAJOR / FACT | Current Blueprint가 첫 검색 시간 분할 직후 완료되어 큰 BP 뒷부분의 참조를 놓칠 수 있음 | `VisualRefSearcher.cpp`의 CurrentBlueprint 분기에서 남은 큐를 유지. `CurrentBlueprintCompletesAllSlices`가 12,002개 노드 중 마지막 참조의 단일 검출과 완료 순서를 검증 |
| MAJOR / FACT | 다른 패키지의 동명 클래스, 복제된 GUID가 함수·변수·디스패처의 오탐 근거가 될 수 있음 | 짧은 클래스 이름 비교를 제거하고 생성 BP·클래스 관계와 GUID 소유자를 함께 확인. `BlueprintSymbolIdentity` 회귀 추가 |
| MAJOR / FACT | 로컬 변수와 멤버 변수 또는 다른 함수의 동명 로컬 변수가 섞일 수 있음 | 선택 노드의 로컬 범위·GUID를 보존하고 BP와 함수 범위로 판정. 로컬 검색에서는 C++ 텍스트 후보 큐 생성 제외 |
| MAJOR / FACT | CustomEvent가 부모 타입 Event 분기에 먼저 걸려 정의 노드가 누락될 수 있음 | CustomEvent를 먼저 처리. 바인딩이 없는 이벤트의 제목 일부를 디스패처 참조로 취급하는 경로도 차단 |
| MAJOR / FACT | 함수 진입 노드의 메뉴가 변수 전용 핸들러에 연결돼 동작하지 않음 | FunctionEntry를 함수 검색 메뉴로 이동하고 메뉴 등록 회귀 추가. 실제 My Blueprint 목록의 메뉴로 사용되지 않는 확장 등록 제거 |
| MAJOR / FACT | 공통 네이티브 부모의 프로퍼티를 쓰는 형제 BP의 self 참조를 놓칠 수 있음 | 선택 프로퍼티의 선언 클래스를 해석하고 후보 클래스와 선언 소유자의 관계로 판정. 형제 BP 네이티브 프로퍼티 회귀 추가 |
| MAJOR / FACT | 기본 BP 검색이 관련 없는 C++ 파일 614개도 순회하며 완료를 지연함 | 실제 콜드 테스트에서 BP 참조는 검출했지만 30초 안에 완료되지 않음. `Include C++ candidates`를 선택 옵션으로 만들고 UI와 검색기 기본값을 꺼짐으로 변경. C++ 성능 테스트는 명시적으로 옵션을 켜서 유지 |
| MINOR / FACT | README가 검증 엔진을 5.8.2로 표기하고 BuildPlugin이 고객 문서·고지를 누락 | 5.8.3으로 수정. README, 오프라인 USER_GUIDE, 개인정보 처리 안내와 고지 문서의 패키징 규칙 추가 |

위 문제는 수정 전 코드 경로에서 확인한 결함이다. 실제 화면을 수동으로
조작해 수정 전후를 비교했다고 주장하지 않는다.

## 자동 검증 기록

- FACT: 1차 격리 QA 호스트 자동화 17개 통과, 종료 코드 0.
  해당 실행 뒤 GUID 충돌·함수 진입 메뉴·네이티브 형제 BP 판정과
  실제 비동기 로딩 테스트를 보강했으므로 이것만으로 최종 통과를 판정하지 않는다.
  로그: `Project/ToyBuilderC/Saved/TraceMotiveBlueprintQAHost/ReleaseFinalTests_20261019.log`.
- FACT: UE 5.8.3 Win64 `BuildPlugin` 성공, 종료 코드 0.
  로그: `Project/ToyBuilderC/Saved/TraceMotiveReleaseFinal_20261019.log`.
- FACT: C++ 기본 스캔이 켜져 있던 콜드 테스트는 30초 완료 제한에 실패했다.
  부모·자식 BP의 GET/SET 수는 각각 2개로 통과했으며, 실패는 완료 대기 항목이었다.
  로그: `Project/ToyBuilderC/Saved/TraceMotiveBlueprintQAHost/ReleaseFinalCold_20261019.log`.
- FACT / PASS: 프로젝트 모듈이 없는 격리 호스트에서 전체 소스를 빌드했고,
  마지막 BP 전용 기본 검색 변경까지 8개 작업을 재컴파일·링크했다. 모두 종료 코드 0.
  최종 로그: `Project/ToyBuilderC/Saved/TraceMotiveReleaseReviewHost/ReleaseBlueprintDefaultBuild_20261019.log`.
- FACT / PASS: 최종 DLL의 플러그인 테스트 16개 및 프로젝트 fixture 생성 2개가
  전부 Success, 종료 코드 0. 변수 참조, 함수·매크로·이벤트·디스패처 식별,
  상속·GUID·로컬 범위, 메뉴 등록, 에셋 근거·재개, 충돌 설정·재개,
  세션 수명·설정·번역·지원 ZIP 등을 포함한다.
  로그: `Project/ToyBuilderC/Saved/TraceMotiveBlueprintQAHost/ReleaseBlueprintDefaultSuite_20261019.log`.
- FACT / PASS: 별도 프로세스의 콜드 테스트에서 소비자 BP가 로드되지 않은 상태를
  확인하고, 실제 에디터 틱·비동기 로딩 경로로 부모 2개·자식 2개 변수 참조를
  검출한 뒤 **729.255ms**에 완료했다. 기본 BP 전용 검색이며 종료 코드 0.
  로그: `Project/ToyBuilderC/Saved/TraceMotiveBlueprintQAHost/ReleaseBlueprintDefaultCold_20261019.log`.
  이는 작은 fixture의 수치이며 대형 고객 프로젝트 성능 수치가 아니다.
- FACT / PASS: 빈 프로젝트 호스트에서 `TraceMotive.Search` 5개가 전부 Success,
  종료 코드 0. 로그: `Project/ToyBuilderC/Saved/TraceMotiveReleaseReviewHost/ReleaseBlueprintDefaultSearch_20261019.log`.
- FACT / PASS: 합성 BP 12,002개 노드의 마지막 참조가 정확히 한 번 검출됐고,
  2개 검색 틱 뒤 완료됐다. QA 호스트의 측정 최대 틱 0.902ms.
  해당 fixture는 대부분 빈 노드이며 Slate 결과 렌더링 비용을 포함하지 않는다.
- FACT / 제한: 명시적으로 C++ 후보를 켠 성능 테스트는 fixture 528개를 포함한
  614개 파일을 큐에 넣고 결과 한도 500개를 반환했다. 큐 구성 7.063ms,
  최대 틱 10.067ms, 수동 틱 루프 전체 680.647ms였다. 실제 에디터의 틱 간격과
  Slate 비용을 포함한 완료 시간은 아니다. 개별 C++ 파일 파싱은 명목 예산을 넘을 수 있다.
- FACT / PASS: 번역 검증 스크립트(런타임 번역 531개), 스타일 아이콘 23개 파일
  존재 검사와 `git diff --check` 통과. 제품 소스·Config·리소스에서 현재 작업장의
  사용자 이름·저장소 절대 경로·프로젝트 이름 패턴은 발견되지 않았다.

## 제출 파일과 로컬 반영

- FACT / PASS: 제출 소스 ZIP은
  `Project/ToyBuilderC/Saved/TraceMotiveSubmissionFinal_20261019/TraceMotive_1.0.0_UE5.8.3_Source.zip`.
  크기 **532,675 bytes**, 단일 `TraceMotive/` 루트 아래 **146개 파일**이다.
  Source, Config, Resources, 번역 콘텐츠 및 고객 문서·고지를 포함한다.
  Binaries, Intermediate, Saved, 로그, 프로젝트 BP와 생성 이미지 미리보기는 제외했다.
- FACT / PASS: ZIP의 모든 파일을 다시 읽어 manifest의 SHA256과 비교했고 불일치 0개.
  소스 파일 80개는 현재 플러그인 및 실제 컴파일한 격리 호스트와 모두 일치했다.
  같은 폴더에 `.manifest.json`, `.zip.sha256`, `release-audit.json`을 저장했다.
- ZIP SHA256: `0E5BC691C6413DC2B7150799FE62FD9B663CCE326B4867C397D394CE3AAD0A98`.
- FACT / PASS: 로컬 프로젝트의 오래된 DLL·PDB·모듈 맵을 최종 빌드로 교체했다.
  로컬 프로젝트, 두 검증 호스트, 빌드 패키지의 DLL 해시는 모두 같았다.
  모듈 맵은 `UnrealEditor-TraceMotive.dll`, Build ID `55116800`을 사용한다.
- DLL SHA256: `627F8A6E26C9238E5E6FB39106A96E32E6CF9B61053D0B4958EEB19184BAE053`.
- 재생성 도구: `Project/ToyBuilderC/Plugins/TraceMotive/Tools/Release/NewTraceMotiveSubmission.ps1`.
  기존 산출물을 덮어쓰지 않으며 신규 출력 폴더를 요구한다.

## 출시 전에 남은 확인

1. **RELEASE BLOCKER / FACT(검증 공백):** 실제 에디터에서 변수·함수·디스패처
   검색, 결과 더블클릭 이동, 범위 변경·재검색·창 종료를 확인한 증거가 없다.
   변수 관찰의 PIE 시작/종료·BP 재컴파일, 충돌 분석의 실제 선택 흐름과
   멀티 PIE도 이번 자동화가 보장하지 않는다.
2. **RELEASE BLOCKER / FACT(확인 자료 부재):** 판매 페이지의 실제 지원 연락처,
   공개 문서 접근성, 게시할 발행자 이름, 아이콘·스토어 미디어의 상업적 사용
   권리 확인 자료가 이번 작업에 제공되지 않았다. `CreatedBy`는 현재 `CJH`다.
   아이콘 파일의 존재·표시 확인은 권리 확인과 다르다.
3. **MAJOR / FACT(성능 범위):** 대형 고객 프로젝트에서 Slate 결과 그래프의
   프레임 시간과 콜드 로딩 성능은 측정하지 않았다. 합성 노드 순회와 작은
   저장 BP fixture의 수치는 이 성능을 대표하지 않는다. BP 참조 결과에는
   C++ 후보의 결과 수 제한이 적용되지 않는다.

## 검색 범위와 주장 한계

기본 Full scan은 `/Game`과 대상 BP 콘텐츠 마운트의 BP를 검사하고,
Asset Registry 역참조와 이미 로드된 BP를 우선 후보로 사용한다.
모든 엔진·외부 플러그인 콘텐츠의 완전성이나 문자열 기반 동적 호출 해석을
보장하는 제품으로 설명하지 않는다. Full scan을 끄면 후보 검색으로 바뀌며
누락 가능성을 툴팁과 문서에 표시한다.

함수·디스패처·매크로·상속의 일부 조합은 합성 노드 테스트로 확인한다.
인터페이스 메시지, 모든 delegate 연결 형태와 실제 결과 이동의 모든 조합을
검증한 것은 아니다. C++ 결과는 의미 기반 심볼 참조가 아닌 텍스트 후보다.
기본 검색은 BP만 검사하며, Project Content에서 `Include C++ candidates`를
켠 경우에만 프로젝트 C++ 소스 후보를 추가한다.

저장 BP fixture 두 개와 ToyBuilderC 생성 소스는 저장소의 ignore 규칙으로
관리 대상에서 제외된다. 로컬 프로젝트에는 있으며, 제출 ZIP에는 넣지 않는다.
`TraceMotive.ProjectFixture.*`와 성능 fixture 테스트는 해당 로컬 자료가 필요하다.
프로젝트 독립적인 검색 회귀는 `TraceMotive.Search`로 실행할 수 있다.

## 제출 형식 근거

2026-10-19에 확인한 [Fab 공식 파일 구조 안내](https://dev.epicgames.com/documentation/en-us/fab/asset-file-format-and-structure-requirements-in-fab)의
코드 모듈·descriptor·Source/Config/Content 구성과 단일 플러그인 ZIP 요구를
파일 감사에 반영했다. ZIP 구성 검사는 Fab의 심사 승인이나 판매 페이지 검수를 뜻하지 않는다.
