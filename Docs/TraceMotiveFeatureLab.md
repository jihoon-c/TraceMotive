# TraceMotive Feature Lab

Open `/Game/TraceMotiveTests/TM_FeatureLab`.

The level uses deliberately obvious names and repeatable timers so TraceMotive results are easy to recognize. The intentional runtime error is test data.

| Feature | Station / asset | Test |
| --- | --- | --- |
| Visual Reference Search / Call Chain | `BP_TM_SecurityDoor`, `ActivateFromKeypad`, `OpenSecurityDoor` | Search the native functions or Blueprint child and inspect the caller-to-target chain. |
| Enhanced Outliner Search | `TM_01_Search_SecurityDoor` | Pair search for `DoorType = Security`, or search tag `TraceMotiveDemo`. |
| Asset Usage Locator | `M_TM_DemoAccent` | Locate its references in the test map and demo actors. |
| Collision Pair Analyzer | `TM_03_Collision_Mover`, `TM_03_Collision_Obstacle` | Select both actors. During PIE the mover alternates swept blocking and clear movement. |
| Click Event Diagnostics | `TM_04_Click_Target` and the PIE widget button | Arm capture, then click the cube or `Click flow test`. |
| Audio Playback Trace | `TM_05_Audio_Source`, `S_TM_TestTone` | Open the trace before PIE; a short tone plays every three seconds. |
| Widget Lifecycle Trace | `TM_06_Widget_Lifecycle` | Open the trace before PIE. `LifecycleStatusText` alternates Visible and Collapsed. |
| Widget Click Flow | PIE widget `FeatureLabClickButton` | Arm the trace and click the button. |
| Instance Reference Trace | `TM_07_Instance_Target` | The controller alternates collision and visibility every two seconds. |
| Runtime Error Trace | `BP_TM_DeliberateError` | Open before PIE. An intentional `Accessed None`-style error is emitted every six seconds. |
| PIE Error Log Analyzer | `BP_TM_DeliberateError` | Stop PIE, then analyze the latest PIE log. |
| Package Progress | `TM_FeatureLab` map | Package the project normally and watch stage/overall progress. The map passes Windows Cook. |
| Global Speed Control | `TM_09_Global_Speed_Target` | Change speed and optionally arm Skip to Target for log substring `BossPhase2`. |
| Context Shortcut Guide | Any Blueprint graph | Open one of the generated Blueprint assets and click its graph. |
| Class Favorites | `BP_TM_FavoriteDebugActor` | Add it from the Content Browser, then open/place/remove it in the favorites palette. |
| Viewport Move Helpers | `TM_10_Viewport_Move_Helper` | Select the actor or its `DemoMesh`, then use actor/component move-to-camera. |

## Runtime markers

Useful Output Log filters:

- `TM_DEMO_COLLISION`
- `TM_DEMO_AUDIO_PLAY`
- `TM_DEMO_WIDGET_`
- `TM_DEMO_REFERENCE_CHANGE`
- `Blueprint Runtime Error`
- `TM_DEMO_SPEED_TARGET_REACHED`

## Regenerating the level

After changing the demo layout, run `Scripts/create_tm_feature_lab.py` through Unreal's Python runner. The script recreates only `/Game/TraceMotiveTests/TM_FeatureLab`; it leaves `Main01` unchanged.
