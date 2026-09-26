# FrameMatch for Nuke

[한국어](README.md) · [English](README.en.md)

변경 영상의 위치와 시간을 원본에 맞추고, 빠진 프레임을 채웁니다.

[![FrameMatch 데모 영상](docs/images/demo-preview.gif)](docs/media/FrameMatch_Demo.mp4)

[▶ 전체 데모 영상](docs/media/FrameMatch_Demo.mp4)

**Align → TimeWarp → RIFE** — 한 노드에서 분석하고, 편집 가능한 Nuke 노드로 내보냅니다. 색상은 변경하지 않습니다.

## 시작하기

1. [최신 릴리스](https://github.com/tardis7732/FrameMatch-for-Nuke/releases/latest)에서 **FrameMatch-Windows.zip**을 받아 압축을 풉니다.
2. NukeX와 [Cattery RIFE](https://github.com/rafaelperez/RIFE-for-Nuke#installation)를 준비합니다. RIFE 모델은 별도 설치입니다.
3. `Open_FrameMatch_NukeX.cmd` 실행 → Tab → **FrameMatch**.

## 사용

- **Change / Source**를 연결하고 범위를 설정합니다.
- 필요하면 기준 한 프레임으로 **Analyze align**. Mask는 Align에만 사용합니다.
- **Analyze timing** → TimeWarp와 RIFE가 자동으로 연결됩니다. 저장된 Align이 없으면 입력 그대로 분석합니다.
- **Export**는 개별 출력, **Export all**은 연결된 전체 체인을 내보냅니다.

## 예제

같은 [릴리스](https://github.com/tardis7732/FrameMatch-for-Nuke/releases/latest)의 **FrameMatch-Sample-Media.zip**을 프로그램 폴더에 풀고 `Open_Demo_NukeX.cmd`를 실행하세요.

**Source 252프레임 + Change 249프레임(ProRes)**과 연결된 예제 프로젝트를 제공합니다. 데모는 실제 Nuke 출력이며, 155프레임을 154·156으로 보간하는 장면을 포함합니다.

[간단 가이드](docs/USAGE.md) · [라이선스 / 출처](THIRD_PARTY.md)

Windows / NukeX 17.0v3에서 확인했습니다. 정방향 단일 샷용이며, 매칭과 보간 결과는 직접 확인하세요. Foundry 공식 플러그인은 아닙니다.