# FrameMatch 사용법

[README](../README.md) · [English](USAGE.en.md)

## 설치와 실행

Windows x64와 NukeX가 필요합니다. **NukeX 17.0v3**에서 확인했습니다. [Cattery RIFE](https://github.com/rafaelperez/RIFE-for-Nuke#installation)를 별도로 설치하고, Nuke의 플러그인 경로에서 `RIFE.cat`을 찾을 수 있도록 설정하세요. 모델 가중치는 이 패키지에 포함하지 않습니다.

릴리스의 **FrameMatch-Windows.zip**을 풀고 다음과 같이 설치합니다.

1. 패키지의 `nuke` 폴더를 사용자 `.nuke` 안에 복사하고 폴더 이름을 `FrameMatch`로 바꿉니다. 일반적인 위치는 `%USERPROFILE%\.nuke\FrameMatch`이며, 그 안에 `menu.py`, `init.py`, Python 파일과 gizmo가 있어야 합니다.
2. 사용자 `.nuke/init.py`에 아래 코드를 추가합니다. 파일이 없으면 만들고, 기존 내용은 유지하세요.

```python
import nuke
nuke.pluginAddPath('./FrameMatch')
```

3. 패키지의 `ofx/FrameMatch.ofx.bundle` 폴더 전체를 `C:\Program Files\Common Files\OFX\Plugins`에 복사합니다. 최종 파일 위치는 `...\Plugins\FrameMatch.ofx.bundle\Contents\Win64\FrameMatch.ofx`입니다.
4. NukeX를 다시 열고 Tab → **FrameMatch**, 또는 **Time → FrameMatch** 메뉴로 생성합니다.

공용 OFX 폴더 대신 환경 변수로 관리하려면 Nuke 실행 전에 `OFX_PLUGIN_PATH`에 패키지의 `ofx` 폴더 절대 경로를 추가하세요. Python 파일도 복사 대신 패키지의 `nuke` 폴더를 `NUKE_PATH`에 추가할 수 있습니다. 기존 환경 변수 값은 유지하세요.

사용자 홈을 별도로 설정한 환경에서는 실제 `.nuke` 위치를 사용하세요. [Foundry 플러그인 설치](https://learn.foundry.com/nuke/developers/80/pythondevguide/installing_plugins.html)와 [OFX 로딩](https://learn.foundry.com/nuke/content/comp_environment/configuring_nuke/loading_ofx_plugins.html) 문서도 참고할 수 있습니다. `examples/FrameMatch.nk`에는 분석 전 노드가 있습니다.

## 예제 열기

1. 같은 릴리스에서 **FrameMatch-Sample-Media.zip**을 받습니다.
2. 프로그램 폴더에 압축을 풉니다. `examples/media/source.mp4`와 `examples/media/change_prores.mov`가 있어야 합니다.
3. NukeX의 **File → Open**으로 `examples/FrameMatch_demo.nk`를 엽니다.
4. Viewer 입력 1은 RIFE 결과, 2는 Source, 3은 TimeWarp입니다. 154–156프레임을 비교하면 155프레임의 보간을 확인할 수 있습니다.

Source는 252프레임, Change는 249프레임입니다. 영상 FPS는 24이며 프로젝트에 분석 데이터와 출력 체인이 저장되어 있습니다.

## 입력과 범위

| 입력 | 연결할 영상 |
| --- | --- |
| Change (0) | 원본에 맞출 수정·생성 영상 |
| Source (1) | 기준이 되는 원본 영상 |
| Mask (2) | Align에 사용할 선택 사항인 마스크 |

먼저 Read에서 정상 재생되는지, 색 공간·해상도·유효 프레임 범위가 맞는지 확인합니다. 파일의 FPS가 같다고 두 영상의 타이밍이 일치하는 것은 아닙니다.

**Change에는 원래 수정 영상을 연결하세요.** 자동 생성한 Transform, TimeWarp, RIFE 등 출력 체인을 분석 입력으로 되돌려 연결하면 중복 보정이나 잘못된 분석이 생길 수 있습니다.

- **Source range**: 최종 출력 타임라인의 시작·끝.
- **Change range**: 대응 프레임을 검색할 입력의 시작·끝.
- 각 줄의 **Reset**: 해당 입력의 범위를 읽습니다.

두 범위의 길이는 달라도 됩니다. 범위를 바꾸면 Timing을 다시 분석해야 합니다. 현재 한 번에 **Source × Change = 1,000,000 프레임 쌍**까지 처리하므로 긴 영상은 샷 단위로 나누세요.

## Align: 위치 맞추기

1. 두 영상의 위치를 비교하기 좋은 한 프레임을 고릅니다.
2. **Reference frame**에 번호를 입력하거나 **Current**로 현재 프레임을 지정합니다.
3. 마스크가 필요하면 Mask 입력을 연결하고 **Use mask**를 켭니다.
4. **Analyze align**을 누릅니다.

실제 Nuke **MatchGrade의 Align Target to Source**를 실행합니다. Change가 Target, 원본이 Source이며 색상 매칭은 수행하지 않습니다. 임시 MatchGrade에서 얻은 Transform·Reformat 설정을 저장한 뒤 임시 노드는 제거합니다.

Mask의 흰 영역은 Change 위에 합성되어 Align을 돕습니다. 마스크는 전체 영상 좌표에 맞춰 연결하세요. Timing 계산에서 픽셀을 제외하는 마스크가 아니며, Timing·홀드 감지·RIFE에서는 사용하지 않습니다.

성공하면 Change 아래에 **Transform → Reformat**을 생성하거나 갱신합니다. Align은 한 프레임에서 얻은 고정 정렬이며 프레임별 트래킹이나 변형 보정이 아닙니다. Align을 변경했다면 Timing도 다시 분석하세요.

## Timing: 프레임 맞추기

**Analyze timing**을 누르면 저장된 Align이 있을 때만 적용해 분석합니다. Align 데이터가 없으면 Change 입력 그대로 비교하며, 자동으로 새 Align 분석을 실행하지 않습니다.

Nuke 내부의 C++ OFX가 영상 특징으로 정방향 대응을 찾고, 서로 다른 프레임 번호에 같은 영상이 반복되는 홀드도 검사합니다. 분석을 위해 별도의 Python 환경이나 중간 이미지 시퀀스를 준비할 필요는 없습니다.

완료되면 **TimeWarp → RIFE**를 자동 생성합니다. 자동 Reformat이 있으면 그 뒤에, 없으면 Change 아래에 연결합니다. FrameMatch 자체는 분석 데이터를 저장하고 Change를 그대로 통과시키므로, 보정 결과는 생성된 출력 체인에 Viewer를 연결해 확인하세요.

## RIFE: 누락 프레임 채우기

내보낸 RIFE Group의 입력은 **하나**이며 해당 **TimeWarp 출력**을 연결합니다. 내부 FrameHold가 같은 입력의 양끝 정상 프레임을 가져옵니다.

| 누락된 출력 프레임 | 양끝 프레임 | 보간 위치 |
| --- | --- | --- |
| 155 | 154, 156 | 1/2 |
| 155, 156, 157 | 154, 158 | 1/4, 2/4, 3/4 |

보간은 TimeWarp 이후의 출력 프레임 기준입니다. 양끝 프레임이 모두 존재하고 영상이 앞으로 진행하는 구간만 채웁니다. 양끝이 없는 시작·끝 구간은 채우지 않으며 **Without endpoints**에 표시합니다. 누락 구간 밖은 TimeWarp 결과를 그대로 통과시킵니다.

**Settings**의 **RIFE GPU / RIFE detail / RIFE resolution**은 새로 생성하거나 내보내는 RIFE에 적용됩니다. 설정 변경 후 기존 출력에 반영하려면 다시 Export하거나 Timing을 재분석하세요. 실제 모델은 설치된 Cattery RIFE를 사용합니다.

## Analyze와 Export의 차이

| 버튼 | 동작 |
| --- | --- |
| Analyze align | 정렬 데이터를 저장하고 연결된 Transform + Reformat을 생성·갱신 |
| Analyze timing | 시간 분석 데이터를 저장하고 연결된 TimeWarp + RIFE를 생성·갱신 |
| Export: Align | Transform + Reformat을 한 세트로 복사 |
| Export: TimeWarp | 저장된 정수 프레임 대응을 nearest 필터의 TimeWarp로 출력 |
| Export: RIFE | 보간 Group만 출력. 해당 TimeWarp 뒤에 연결 |
| Export all | Align → TimeWarp → RIFE 체인. Align 데이터가 없으면 TimeWarp → RIFE |

수동 Export는 **첫 입력이 연결되지 않은 독립 복사본**입니다. Export all은 내보낸 노드끼리 순서대로 연결하고 원래 Change에는 연결하지 않습니다.

재분석은 이 FrameMatch가 자동 생성한 출력만 갱신하며, 기존 후속 연결을 유지합니다. 수동으로 Export한 복사본은 유지됩니다. 분석 노드를 삭제해도 Export 결과는 남지만 RIFE를 실행하려면 패키지의 래퍼와 별도 설치한 모델이 계속 필요합니다.

## Results 읽기

| 표시 | 의미 |
| --- | --- |
| Align | 저장된 기준 프레임 또는 None |
| Matched / 전체 | 누락·영상 홀드로 분류되지 않은 Source 프레임 수 |
| Missing | 프레임 대응의 빈 구간과 감지된 영상 홀드 |
| RIFE | 양끝 프레임이 있어 보간할 수 있는 출력 프레임 수 |
| Without endpoints | 양끝이 없어 보간할 수 없는 프레임 수 |
| Review | 대응이 모호해 직접 확인할 Change 프레임 수 |

이 값은 분석 후보의 개수이며 정답률이나 오류 확률이 아닙니다. 빠른 움직임, 가림, 수정된 물체, 구간 경계는 직접 확인하세요.

## 문제 해결

| 증상 | 확인할 내용 |
| --- | --- |
| FrameMatch 메뉴가 없음 | `.nuke/init.py`의 등록 경로 또는 `NUKE_PATH`를 확인하고 Nuke 재시작 |
| OFX 노드를 찾지 못함 | 공용 OFX 폴더의 bundle 위치 또는 시작 전 `OFX_PLUGIN_PATH` 설정 확인. Windows x64 바이너리 사용 |
| MatchGrade가 없거나 Align 실패 | NukeX 라이선스와 두 입력의 기준 프레임 확인 |
| RIFE 또는 RIFE.cat을 찾지 못함 | Cattery RIFE 설치와 모델 검색 경로 확인 |
| `outputFrameText` validate 경고 | 현재 패키지로 다시 시작하고 이전 RIFE 출력을 재생성 |
| Cannot fetch input frame | 입력 범위, Read 디코딩, 상위 노드 오류 확인 |
| 정렬이 두 번 적용됨 | 이미 정렬된 영상을 저장된 Align에 다시 입력하지 않았는지 확인 |
| 홀드 일부를 놓침 | 노이즈·생성 변화가 감지 기준을 넘을 수 있으므로 실제 영상을 비교 |
| 시작·끝 누락이 안 채워짐 | 양끝 정상 프레임이 없으면 자동 보간하지 않음 |

컷이나 역재생은 분리해서 작업하세요. 입력을 바꾸면 다시 분석해야 합니다.

## 소스 빌드

Visual Studio 2022 C++ Build Tools, Windows SDK, CMake 설치 후 `build_windows.cmd`를 실행합니다. 빌드 후 네이티브 테스트도 실행합니다. 다른 Nuke 버전에서의 호환성은 별도로 확인해야 합니다.
