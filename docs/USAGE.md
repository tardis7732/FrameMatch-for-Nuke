# 간단 가이드

[README](../README.md) · [English](USAGE.en.md)

## 연결과 분석

1. **Change**에 수정 영상, **Source**에 원본을 연결합니다. 범위 옆 **Reset**으로 입력 범위를 읽습니다.
2. 위치가 다르면 **Reference frame**을 정하고 **Analyze align**을 누릅니다. **Current**는 현재 프레임을 선택합니다. 선택 사항인 **Mask / Use mask**는 Align에만 적용됩니다.
3. **Analyze timing**을 누르면 저장된 Align을 사용해 분석하고 **TimeWarp → RIFE**를 연결합니다. Align 데이터가 없으면 입력 그대로 분석합니다.
4. Viewer에서 결과를 확인합니다. 분석 노드의 Change 입력에는 원래 수정 영상을 유지하세요.

## 출력

- **Align**: Transform + Reformat 한 세트.
- **TimeWarp**: 원본 시간에 대응하는 Change 프레임.
- **RIFE**: 누락 구간만 양끝 정상 프레임으로 보간. 입력은 하나이며 TimeWarp 뒤에 연결합니다.
- **Export all**: 위 노드를 순서대로 연결한 체인.

Analyze는 연결된 출력을 만들거나 갱신합니다. 수동 Export는 첫 입력이 연결되지 않은 별도 복사본을 만듭니다. 분석 노드 자체는 Change를 그대로 통과시킵니다.

155가 비면 154·156으로 하나를, 155–157이 비면 154·158로 세 프레임을 만듭니다. 양끝이 없는 구간은 보간하지 않습니다.

## Results

**Matched / Missing**은 매칭된 프레임과 누락·홀드 후보 수입니다. **RIFE / Without endpoints**는 보간 가능 프레임과 양끝이 없는 프레임, **Review**는 직접 확인할 대응입니다. 정확도 점수는 아닙니다.

## 실행 참고

예제 미디어 ZIP을 프로그램 폴더에 풀고 `Open_Demo_NukeX.cmd`를 실행합니다. 예제 파일은 `examples/FrameMatch_demo.nk`입니다.

Nuke 경로를 직접 지정하려면:

```powershell
.\Open_FrameMatch_NukeX.cmd -NukeExe 'C:\Program Files\Nuke17.0v3\Nuke17.0.exe'
```

기존 런처를 사용한다면 Nuke 실행 전에 패키지의 `nuke`를 `NUKE_PATH`, `ofx`를 `OFX_PLUGIN_PATH`에 추가하세요. RIFE 모델을 못 찾으면 [Cattery RIFE 설치](https://github.com/rafaelperez/RIFE-for-Nuke#installation)를 확인하세요.

입력 오류가 나면 Read의 재생과 범위를 먼저 확인하세요. 컷·역재생은 샷을 나누고, Source × Change는 1,000,000 프레임 쌍 이하로 설정하세요.

소스 빌드: Visual Studio 2022 C++ Build Tools, Windows SDK, CMake 설치 후 `build_windows.cmd`.