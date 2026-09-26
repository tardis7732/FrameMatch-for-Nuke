# Third-party components

| Component | Use | Notice |
| --- | --- | --- |
| [OpenFX](https://github.com/AcademySoftwareFoundation/openfx) | Headers used to build the native timing analyzer | [BSD 3-Clause](licenses/OpenFX-BSD-3-Clause.txt) |
| [RIFE for Nuke](https://github.com/rafaelperez/RIFE-for-Nuke) by Rafael Silva | `nuke/FrameMatchRIFE.gizmo`, derived from Cattery RIFE 1.1.1 | [Upstream MIT license](licenses/RIFE-MIT.txt) |
| Nuke / NukeX / MatchGrade / Inference | Host and native nodes | Installed separately; Foundry software is not redistributed |

The RIFE wrapper retains the original interpolation graph and author credits.
Changes: distinct `FrameMatchRIFE` class name, a pure `outputFrame` expression,
and a read-only numeric output-frame display in place of a text field mutated
during validation. The model is resolved from the user's installed `RIFE.cat`.
No model weights or Cattery runtime are bundled.

RIFE model and training-data terms are separate from this project's code license.
See the [upstream notice](https://github.com/rafaelperez/RIFE-for-Nuke#license-and-acknowledgments),
[ECCV2022-RIFE](https://github.com/megvii-research/ECCV2022-RIFE), and
[Practical-RIFE](https://github.com/hzwer/Practical-RIFE).

Demo imagery is included only to demonstrate this tool and is not offered as a reusable footage library.
