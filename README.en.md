# FrameMatch for Nuke

[한국어](README.md) · [English](README.en.md)

Align an edited clip to its original timeline and fill missing frames.

[![FrameMatch demo video](docs/images/demo-preview.gif)](docs/media/FrameMatch_Demo.mp4)

[Watch the full demo](docs/media/FrameMatch_Demo.mp4)

**Align → TimeWarp → RIFE** — analyze in one node, then export editable Nuke nodes. No color matching is applied.

## Get started

1. Download **FrameMatch-Windows.zip** from the [latest release](https://github.com/tardis7732/FrameMatch-for-Nuke/releases/latest) and extract it.
2. Install NukeX and [Cattery RIFE](https://github.com/rafaelperez/RIFE-for-Nuke#installation). RIFE model weights are installed separately.
3. Run `Open_FrameMatch_NukeX.cmd` → Tab → **FrameMatch**.

## Use

- Connect **Change / Source** and set the ranges.
- Optionally run **Analyze align** using one reference frame. Mask is used only for Align.
- **Analyze timing** creates and connects TimeWarp and RIFE. Without stored Align data, it analyzes the input as-is.
- **Export** creates individual outputs; **Export all** creates a connected chain.

## Example

Extract **FrameMatch-Sample-Media.zip** from the same [release](https://github.com/tardis7732/FrameMatch-for-Nuke/releases/latest) into the package folder, then run `Open_Demo_NukeX.cmd`.

Includes **252 Source frames + 249 Change frames (ProRes)** and a connected example project. The demo uses real Nuke renders, including frame 155 interpolated from frames 154 and 156.

[Quick guide](docs/USAGE.en.md) · [Licenses / credits](THIRD_PARTY.md)

Tested on Windows / NukeX 17.0v3. Intended for a single forward-moving shot; inspect matching and interpolation results. Not an official Foundry plugin.