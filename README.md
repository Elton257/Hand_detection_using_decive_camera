# HandBoard

Draw on the whiteboard in **Microsoft Teams, Slack, Google Meet** (or any app you can draw in with a mouse) using your hand in front of the laptop camera. The right hand draws and erases, the left hand zooms. No mouse needed.

Hands are tracked with **OpenCV DNN** and the **MediaPipe** hand models from [OpenCV Zoo](https://github.com/opencv/opencv_zoo), so it works regardless of skin color, lighting, background or your face being in the frame. No calibration is needed.

## Gestures

| Hand  | Gesture                         | Action                                   |
|-------|---------------------------------|------------------------------------------|
| Right | Index finger only               | Move the cursor (no drawing)             |
| Right | Index finger + thumb out        | **Draw** (left mouse button held)        |
| Right | Index + middle finger (V)       | **Erase** (switches to the eraser tool)  |
| Right | Fist / open hand / not visible  | Nothing (mouse button released)          |
| Left  | 1 finger (index)                | Zoom in (`Ctrl` + mouse wheel up)        |
| Left  | 2 fingers (V)                   | Zoom out (`Ctrl` + mouse wheel down)     |

A gesture must be seen for a few consecutive frames before it takes effect, so a single bad frame never starts or stops a stroke.

## Keyboard shortcuts

All shortcuts are global, so they work while Teams/Slack/Meet has focus. On some laptops you also need to hold `Fn`.

| Shortcut           | Action                                              |
|--------------------|-----------------------------------------------------|
| `Ctrl+Alt+F8`      | Start / pause hand control (starts **paused**)      |
| `Ctrl+Alt+F9`      | Save the **pen** tool position (hover the pen icon first)    |
| `Ctrl+Alt+F10`     | Save the **eraser** tool position (hover the eraser icon first) |
| `Ctrl+Alt+F11`     | Save a camera snapshot next to the exe (diagnostics) |
| `Ctrl+Alt+F7`      | Swap hand roles (left-handed use)                   |
| Window ✕ button    | Quit                                                |

Shortcuts use F-keys on purpose. On Windows the `AltGr` key is the same as `Ctrl+Alt`, so letter shortcuts like `Ctrl+Alt+Q` would fire whenever you type characters such as `@` or `\` with `AltGr`.

## Requirements

- Windows 10 or 11, 64-bit
- A webcam
- Visual Studio 2022 Build Tools (C++ workload) and CMake
- OpenCV **4.7 or newer** (4.14.0 is used by default)

`rregullo.bat` installs all of these for you.

## Quick start

1. Double-click **`rregullo.bat`**. It:
   - sets up the VS Code configuration,
   - installs CMake and the C++ Build Tools if they are missing (via `winget`),
   - downloads OpenCV 4.14.0 (~200 MB) and extracts it to `C:\opencv-4.14.0` (~1 GB),
   - downloads the two hand models into `models/` if they are missing,
   - builds the program.
2. Run **`build\Release\HandBoard.exe`** (or press `F5` in VS Code).
3. Open the whiteboard and select the pen. Hover the pen icon and press `Ctrl+Alt+F9`, then hover the eraser icon and press `Ctrl+Alt+F10`.
4. Press `Ctrl+Alt+F8` and start drawing.

To rebuild after changing the code: run `build.bat`, or press `Ctrl+Shift+B` in VS Code.

### Manual build

```bat
cmake -B build -DOpenCV_DIR=C:/opencv-4.14.0/opencv/build
cmake --build build --config Release
```

The build copies `opencv_world4140.dll` and the `models/` folder next to the exe. To run HandBoard on another PC, copy the whole `build\Release` folder. That PC needs the Microsoft Visual C++ Redistributable (x64), which most PCs already have.

## How it works

1. **Camera**: frames come from Media Foundation (built into Windows) and are mirrored, like looking into a mirror.
2. **Palm detector** (`palm_detection_mediapipe_2023feb.onnx`, 192×192) finds the palms in the frame.
3. **Hand landmark model** (`handpose_estimation_mediapipe_2023feb.onnx`, 224×224) returns 21 landmarks per hand (every joint of every finger) and whether it is a left or right hand.
4. **Tracking**: once a hand is found, the next frame is searched directly around its landmarks without the palm detector, the same way MediaPipe does it. This roughly halves the work per frame.
5. **Gestures**: a finger counts as extended when it is straight at its middle joint and its tip is farther from the wrist than that joint. This doesn't depend on hand rotation or size.
6. **Cursor**: the fingertip position goes through a One-Euro filter (steady when the hand rests, no lag when it moves fast) and is sent to Windows with `SendInput`.

The pre/post-processing in `HandDetector.cpp` is a C++ port of OpenCV Zoo's `mp_palmdet.py` and `mp_handpose.py`. It matches the official Python output to within 0.5 px.

Measured on real webcam photos: hands found in 6/6 photos, 30/30 fingers read correctly, about 18 ms per frame while tracking, and RAM steady at about 52 MB after 600 frames.

## Project layout

```
HandBoard/
├── main.cpp            Entry point (WinMain): window + worker thread
├── CMakeLists.txt      Top-level build: OpenCV, lib, src, exe
├── lib/                Headers (.hpp)            -> handboard_headers (INTERFACE)
├── src/                Implementations (.cpp)    -> handboard_core (static library)
├── models/             The two ONNX models + their Apache 2.0 license
├── build.bat           Build script (also used by VS Code)
└── rregullo.bat        One-click setup: tools, OpenCV, models, build
```

Everything is organized in classes, with no global variables. `Application` owns all the parts, and the window and the worker thread only communicate through `SharedState`.

```
Application
├── SharedState      thread-safe flags, preview image, status text
├── MainWindow       UI thread: preview window + Ctrl+Alt+F-key shortcuts
└── Worker           worker thread, one frame at a time:
    ├── Camera             webcam frames (Media Foundation)
    ├── HandDetector       palm + 21 landmarks (OpenCV DNN), with tracking
    ├── GestureClassifier  landmarks -> Point / Draw / Erase / Zoom
    ├── Debounce, OneEuro  flicker-free gestures, smooth cursor
    ├── InputSender        mouse + Ctrl+wheel (SendInput)
    └── PreviewRenderer    camera image + hand skeleton
```

| Class               | Responsibility                                                  |
|---------------------|-----------------------------------------------------------------|
| `Application`       | Single-instance check, crash logging, starts/stops everything   |
| `SharedState`       | Everything the two threads share (atomics + one mutex)          |
| `MainWindow`        | Always-on-top preview window, global shortcuts                  |
| `Worker`            | Main loop: read frame, detect, classify, act, publish           |
| `Camera`            | Webcam capture (Media Foundation)                               |
| `HandDetector`      | Palm detection + 21 landmarks with OpenCV DNN                   |
| `GestureClassifier` | Which fingers are extended, which gesture that is               |
| `PreviewRenderer`   | Draws the preview image                                         |
| `InputSender`       | Virtual mouse and keyboard                                      |
| `Logger`, `Clock`, `Paths` | `HandBoard.log`, timing, exe folder                      |
| `Config` (namespace) | All tunable settings                                           |

## Configuration

Everything you might want to tune is in [`lib/Config.hpp`](lib/Config.hpp), for example:

- `AREA_X0 … AREA_Y1`: the part of the camera image that maps to the whole screen (shown as a yellow box in the preview)
- `THUMB_OUT_RATIO`: how far the thumb must stick out to count as "draw"
- `FRAMES_TO_PRESS` / `FRAMES_TO_RELEASE`: how many frames a gesture must hold
- `EURO_MINCUT` / `EURO_BETA`: cursor smoothing
- `DNN_THREADS`: CPU threads OpenCV may use (lower it if meetings get slow)

## Troubleshooting

- **The program closed by itself.** Open `build\Release\HandBoard.log`. It records start, exit and the reason (window closed, Windows shutdown, or a crash with its code), plus camera problems and OpenCV errors.
- **"Kamera nuk u gjet" (camera not found).** Another app may be holding the camera exclusively. Most webcams can be shared on Windows 10/11, but some can't.
- **"Modelet mungojnë" (models missing).** The `models` folder must be next to `HandBoard.exe`. Rebuild, or run `rregullo.bat`.
- **Ctrl+Alt+F-keys do nothing.** Try holding `Fn` too, or check `HandBoard.log` for shortcuts that another program already uses.
- **A gesture isn't recognized.** Press `Ctrl+Alt+F11` during the gesture to save snapshots (`HandBoard_foto_N.bmp`) next to the exe. They are useful for tuning the thresholds.

## Notes

- On-screen messages in the app are in Albanian.
- Code comments are in English.

## License and credits

- Hand models: [OpenCV Zoo](https://github.com/opencv/opencv_zoo), `palm_detection_mediapipe` and `handpose_estimation_mediapipe`, Apache License 2.0 (see `models/LICENSE-Apache-2.0.txt`). Originally from Google MediaPipe.
- [OpenCV](https://opencv.org), Apache License 2.0.
