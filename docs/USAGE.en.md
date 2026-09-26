# FrameMatch guide

[README](../README.en.md) · [한국어 가이드](USAGE.md)

## Install and launch

Use Windows x64 and NukeX. This package was tested with **NukeX 17.0v3**. Install [Cattery RIFE](https://github.com/rafaelperez/RIFE-for-Nuke#installation) separately; its `RIFE.cat` must be discoverable through Nuke's plugin paths. Model weights are not bundled.

Extract **FrameMatch-Windows.zip** and run `Open_FrameMatch_NukeX.cmd`, then use Tab → **FrameMatch**, or **Time → FrameMatch**. The launcher sets paths for this session; it does not edit `.nuke` or permanent environment settings.

To select a specific Nuke installation:

```powershell
.\Open_FrameMatch_NukeX.cmd -NukeExe 'C:\Program Files\Nuke17.0v3\Nuke17.0.exe'
```

For a studio launcher, add the package's `nuke` directory to `NUKE_PATH` and its `ofx` directory to `OFX_PLUGIN_PATH` **before Nuke starts**. `examples/FrameMatch.nk` contains a fresh analysis node.

## Open the example

1. Download **FrameMatch-Sample-Media.zip** from the same release.
2. Extract it into the package folder. Check for `examples/media/source.mp4` and `examples/media/change_prores.mov`.
3. Run `Open_Demo_NukeX.cmd` to open `examples/FrameMatch_demo.nk`.
4. Viewer input 1 is the RIFE result, 2 is Source, and 3 is TimeWarp. Compare frames 154–156 to inspect interpolation at frame 155.

Source contains 252 frames and Change contains 249, at 24 fps. The project includes stored analysis and connected outputs.

## Inputs and ranges

| Input | Content |
| --- | --- |
| Change (0) | Edited/generated clip to correct |
| Source (1) | Original reference clip |
| Mask (2) | Optional full-frame alignment aid |

Check decoding, color space, dimensions, and available frames. FrameMatch sees Nuke-evaluated images; matching container FPS alone does not align their timing.

Keep **Change connected to the original edited clip**. Do not feed the automatic Transform, TimeWarp, RIFE, or downstream outputs back into the analysis node.

- **Source range** defines the output timeline.
- **Change range** defines the searchable input.
- **Reset** reads the corresponding input's range.

The ranges can have different lengths. Changing ranges invalidates Timing data. The current limit is **1,000,000 Source × Change frame pairs**. Work shot by shot.

## Align

1. Choose a clear frame for comparing the two inputs.
2. Enter the **Reference frame**, or press **Current**.
3. Connect a Mask and enable **Use mask** if helpful.
4. Click **Analyze align**.

The actual MatchGrade **Align Target to Source** action runs with Change as Target and the original as Source. No color matching is performed. The temporary MatchGrade is removed after its Transform and Reformat settings are stored.

White Mask regions are composited over Change as an alignment aid. Supply full-frame coordinates. This is not a mathematical pixel-exclusion mask for Timing. Timing, hold detection, and RIFE do not use Mask.

On success, **Change → Transform → Reformat** is created or updated. Alignment is one fixed spatial transform, not a per-frame tracker or deforming/perspective warp. Rerun Timing after changing Align.

## Timing

**Analyze timing** uses saved alignment if available; otherwise it compares raw Change. It does not automatically solve a new Align.

Native C++ OFX extracts image features, solves a monotone correspondence, and checks for repeated images at different frame numbers. Analysis needs no external Python environment or intermediate image sequence.

**TimeWarp → RIFE** is created automatically, after the automatic Reformat when present, otherwise after Change. FrameMatch stores analysis and passes Change through unchanged; connect the Viewer to the generated output chain to inspect the correction.

## RIFE

The exported RIFE Group has **one input**, expecting the associated **TimeWarp output**. Internal FrameHold nodes sample two times from this input.

| Missing output frames | Endpoints | Interpolation positions |
| --- | --- | --- |
| 155 | 154, 156 | 1/2 |
| 155, 156, 157 | 154, 158 | 1/4, 2/4, 3/4 |

These are output frame numbers after TimeWarp. Both endpoints must exist and advance through the clip. Unresolved head/tail gaps remain unfilled and appear under **Without endpoints**. Frames outside gap intervals pass the TimeWarp result through.

**Settings → RIFE GPU / RIFE detail / RIFE resolution** controls newly created/exported RIFE nodes. Export again or rerun Timing to update existing output settings. The model comes from the installed Cattery RIFE.

## Analyze and Export

| Button | Behavior |
| --- | --- |
| Analyze align | Stores alignment and creates/updates connected Transform + Reformat |
| Analyze timing | Stores timing and creates/updates connected TimeWarp + RIFE |
| Export: Align | Exports Transform + Reformat as one set |
| Export: TimeWarp | Exports the stored integer lookup using nearest filtering |
| Export: RIFE | Exports interpolation alone; connect the corresponding TimeWarp |
| Export all | Creates Align → TimeWarp → RIFE, or TimeWarp → RIFE when no alignment is stored |

Manual exports are **independent copies with the first input disconnected**. Export all connects its nodes together but does not connect to the original Change.

Reanalysis updates only this tool's automatic outputs and preserves downstream connections. Manual exports are retained. Deleting the analysis node does not delete exports; the local RIFE wrapper and separately installed model are still required.

## Results

| Field | Meaning |
| --- | --- |
| Align | Stored reference frame or None |
| Matched / total | Source timestamps not classified as missing or held images |
| Missing | Mapping gaps plus detected image holds |
| RIFE | Missing output frames with usable endpoints |
| Without endpoints | Missing frames that cannot be interpolated |
| Review | Change frames with ambiguous correspondence, for manual inspection |

These are candidates, not ground-truth accuracy or error probabilities. Inspect boundaries, fast motion, occlusions, and edited objects.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| No FrameMatch menu | Start through the launcher; verify `NUKE_PATH` |
| Unknown OFX | Set `OFX_PLUGIN_PATH` before startup; use the Windows x64 binary |
| MatchGrade unavailable / Align fails | Use NukeX with a valid license; check both reference frames |
| Missing RIFE / RIFE.cat | Install Cattery RIFE and register its model search path |
| `outputFrameText` validate warning | Restart with this package and regenerate older RIFE exports |
| Cannot fetch input frame | Check valid ranges, decoding, and upstream errors |
| Alignment applied twice | Do not feed an already aligned clip into stored alignment again |
| Some holds are missed | Noise or generated changes may exceed detection thresholds; compare the actual images |
| Unfilled head/tail gap | No synthesis without two valid endpoints |

Split cuts and reverse playback into separate shots. Reanalyze after changing inputs.

## Build

Install Visual Studio 2022 C++ Build Tools, Windows SDK, and CMake, then run `build_windows.cmd`. The build also runs native tests. Compatibility with other Nuke versions needs separate verification.