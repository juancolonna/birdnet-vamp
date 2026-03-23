# 🎶 🐦‍⬛ BirdNet VAMP Plugin for Audacity

A VAMP plugin for Audacity that runs [BirdNET v2.4](https://github.com/birdnet-team/birdnet) inference to automatically detect and label bird vocalizations in audio recordings.

Detections appear as labeled regions directly on the Audacity track, with the species name and confidence score. Consecutive or overlapping detections of the same species are automatically merged into a single label.

## Screenshot

![BirdNET VAMP Plugin in Audacity](assets/screenshot.png)

---

## Features

- Automatic bird species detection using BirdNET v2.4 (TensorFlow backend)
- Labels appear directly on the Audacity track with species name and confidence score
- Three configurable parameters via Audacity's plugin interface:
  - **Confidence Threshold** — minimum confidence score to report a detection (default: 0.5)
  - **Top K Species** — maximum number of species candidates per segment (default: 3)
  - **Stride (s)** — sliding window step size in seconds (default: 3.0)
- Works on full recordings or selected segments
- Consecutive and overlapping detections of the same species are merged automatically

---

## Requirements

- Ubuntu 22.04
- [Miniconda](https://docs.conda.io/en/latest/miniconda.html) installed at `~/miniconda3`
- `cmake`, `g++`, and `vamp-plugin-sdk` (installed automatically by `install.sh`)

---

## Installation

> **Note:** This repository uses Git LFS to store the Audacity AppImage.
> Make sure Git LFS is installed before cloning: `sudo apt install git-lfs`

### 1. Clone the repository

```bash
sudo apt install git-lfs
git clone https://github.com/juancolonna/birdnet-vamp-plugin.git
cd birdnet-vamp
```

### 2. Run the installation script

```bash
bash install.sh
```

The script will automatically:
- Install system build dependencies (`cmake`, `g++`, `vamp-plugin-sdk`)
- Create a Conda environment named `birdnet-plugin` with Python 3.12
- Install the `birdnet` Python package inside the Conda environment
- Compile the VAMP plugin into the `build/` directory
- Copy `birdnet_run.py` into `build/` alongside the plugin
- Create a desktop shortcut named **Audacity-BirdNet** in your application menu

> **Note:** The installation does not modify or remove any existing Audacity installation on your system. The bundled AppImage runs independently.

---

## Running

### From the application menu

After installation, open **Audacity-BirdNet** from your application menu. The shortcut automatically sets `VAMP_PATH` to the correct directory.

### From the terminal

```bash
VAMP_PATH=$PWD/build ./audacity-linux-3.7.7-x64-22.04.AppImage
```

> Run this command from inside the cloned repository directory.

---

## Usage

1. Open an audio file in Audacity (**File → Open**)
2. Optionally select a specific region of the track to analyze
3. Go to **Analyze → BirdNET**
4. Adjust parameters if desired:
   - **Confidence Threshold**: raise to reduce false positives (e.g., 0.7), lower to increase recall (e.g., 0.3)
   - **Top K Species**: number of candidate species evaluated per 3-second segment
   - **Stride (s)**: step size of the sliding window — smaller values produce more detections but increase processing time
5. Click **OK** and wait for the analysis to complete
6. Detections appear as labeled regions on a new label track

> **Tip:** The output label track can be exported via **File → Export → Export Labels** for further analysis.

---

## Output format

Each label on the track follows the format:

```
Species Name (XX%)
```

For example:
```
Black-capped Chickadee (82%)
House Finch (61%)
```

Where `XX%` is the average confidence score across all merged segments.

---

## Project structure

```
birdnet-vamp-plugin/
├── BirdNetPlugin.cpp                          # VAMP plugin implementation (C++)
├── BirdNetPlugin.h                            # VAMP plugin header
├── birdnet_run.py                             # BirdNET inference script (Python)
├── CMakeLists.txt                             # Build configuration
├── install.sh                                 # Installation script
├── audacity-linux-3.7.7-x64-22.04.AppImage   # Bundled Audacity AppImage
└── build/                                     # Compiled plugin (created by install.sh)
    ├── birdnet-vamp.so                        # Compiled VAMP plugin
    └── birdnet_run.py                         # Copy of the inference script
```

---

## How it works

1. When **Analyze → BirdNET** is triggered, the VAMP plugin accumulates all audio samples into a buffer
2. At the end of the stream, it writes the buffer to a temporary WAV file
3. It invokes `birdnet_run.py` as a subprocess using the Python interpreter from the `birdnet-plugin` Conda environment
4. The Python script runs BirdNET v2.4 inference and returns detections as a JSON array via stdout
5. Consecutive or overlapping detections of the same species are merged into single labels
6. The plugin reads the JSON, creates VAMP features, and displays them as labeled regions in Audacity
7. The temporary WAV file is deleted after processing

---

## Troubleshooting

**Plugin does not appear in Analyze menu**
- Make sure `VAMP_PATH` points to the `build/` directory
- Re-run `bash install.sh` to recompile and reconfigure

**No detections produced**
- Try lowering the **Confidence Threshold** (e.g., 0.1)
- Make sure the audio contains bird vocalizations
- Check that the Conda environment is correctly installed: `conda activate birdnet-plugin && python3 -c "import birdnet; print('OK')"`

**Audacity shows "not responding" during analysis**
- This is expected — BirdNET inference with TensorFlow can take 10–30 seconds depending on audio length
- Click **Wait** and the analysis will complete normally

---

## Citation

If you use this plugin in your research, please cite:

```bibtex
@software{colonna2024birdnet_vamp,
  author  = {Colonna, Juan G.},
  title   = {BirdNET VAMP Plugin for Audacity},
  year    = {2024},
  url     = {https://github.com/juancolonna/birdnet-vamp-plugin}
}
```

---

## License and Author

MIT License — see [LICENSE](LICENSE) for details.

**Prof. Dr. Juan G. Colonna** — [github.com/juancolonna](https://github.com/juancolonna)
