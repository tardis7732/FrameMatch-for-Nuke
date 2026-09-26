# Quick guide

[README](../README.en.md) · [한국어](USAGE.md)

## Connect and analyze

1. Connect the edited clip to **Change** and the original to **Source**. **Reset** reads input ranges.
2. If alignment is needed, choose a **Reference frame** and click **Analyze align**. **Current** selects the current frame. Optional **Mask / Use mask** applies only to Align.
3. **Analyze timing** uses stored alignment and connects **TimeWarp → RIFE**. Without alignment data, it analyzes the input as-is.
4. Inspect the result in the Viewer. Keep the original edited clip connected to the analysis node's Change input.

## Outputs

- **Align**: Transform + Reformat as one set.
- **TimeWarp**: Change frames mapped onto the Source timeline.
- **RIFE**: interpolation only within missing intervals, using their valid endpoints. One input, connected after TimeWarp.
- **Export all**: these nodes connected in order.

Analyze creates or updates connected outputs. Manual Export creates independent copies with the first input disconnected. The analysis node itself passes Change through.

One missing frame at 155 uses endpoints 154 and 156. Three missing frames at 155–157 use 154 and 158. Gaps without both endpoints remain unfilled.

## Results

**Matched / Missing** counts matched frames and missing or held-image candidates. **RIFE / Without endpoints** counts fillable frames and those lacking endpoints. **Review** flags uncertain correspondences to inspect; these are not accuracy scores.

## Launch notes

Extract the sample media ZIP into the package folder and run `Open_Demo_NukeX.cmd`. The project is `examples/FrameMatch_demo.nk`.

To choose a Nuke installation:

```powershell
.\Open_FrameMatch_NukeX.cmd -NukeExe 'C:\Program Files\Nuke17.0v3\Nuke17.0.exe'
```

For an existing launcher, add the package's `nuke` folder to `NUKE_PATH` and `ofx` to `OFX_PLUGIN_PATH` before Nuke starts. If model weights are missing, check the [Cattery RIFE installation](https://github.com/rafaelperez/RIFE-for-Nuke#installation).

For input errors, check Read playback and valid ranges. Split cuts or reverse playback into separate shots. Keep Source × Change within 1,000,000 frame pairs.

Build from source with Visual Studio 2022 C++ Build Tools, Windows SDK, and CMake, then run `build_windows.cmd`.