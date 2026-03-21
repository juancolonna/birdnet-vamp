#!/usr/bin/env python3
"""
birdnet_run.py — BirdNET inference script for the Audacity VAMP plugin.

This script is called by the VAMP plugin (BirdNetPlugin.cpp) as a subprocess.
It loads a BirdNET acoustic model, runs species prediction on a WAV file,
and prints the results as a JSON array to stdout.

Usage:
    python3 birdnet_run.py <wav_path> [threshold] [top_k] [stride]

Arguments:
    wav_path   : Path to the input WAV file.
    threshold  : Minimum confidence score to report a detection (default: 0.1).
    top_k      : Maximum number of species to consider per segment (default: 3).
    stride     : Sliding window step in seconds, in range [0.1, 3.0] (default: 3.0).

Output:
    JSON array of detections, each containing:
        - species    : Common name of the detected species.
        - scientific : Scientific name of the detected species.
        - confidence : Confidence score rounded to 4 decimal places.
        - time_s     : Start time of the detection segment in seconds.
"""

import sys
import json
import birdnet


def main():
    # Parse command-line arguments
    wav_path  = sys.argv[1]
    threshold = float(sys.argv[2]) if len(sys.argv) > 2 else 0.5
    top_k     = int(sys.argv[3])   if len(sys.argv) > 3 else 3
    stride    = float(sys.argv[4]) if len(sys.argv) > 4 else 3.0

    # Clamp stride to valid range and compute overlap
    stride  = max(0.1, min(3.0, stride))  # ensure stride is in [0.1, 3.0]
    overlap = max(0.0, 3.0 - stride)      # overlap = window_duration - stride

    # Load BirdNET acoustic model v2.4 with TensorFlow backend
    model = birdnet.load("acoustic", "2.4", "tf")

    # Run prediction with sliding window
    result = model.predict(
        wav_path,
        default_confidence_threshold=threshold,
        top_k=top_k,
        overlap_duration_s=overlap,
    )

    # Extract prediction tensors and metadata
    probs   = result.species_probs[0]  # shape: [n_segments, top_k]
    ids     = result.species_ids[0]    # shape: [n_segments, top_k] — indices into species_list
    species = result.species_list      # full list of species labels ("Scientific_Common")
    hop_dur = result.hop_duration_s    # actual step size in seconds (= stride)

    detections = []
    for seg_idx in range(len(probs)):
        time_s = seg_idx * hop_dur
        for k in range(probs.shape[1]):
            conf = float(probs[seg_idx][k])
            if conf < threshold:
                continue

            # Map index to species label and split into scientific / common name
            sp_idx     = int(ids[seg_idx][k])
            label      = species[sp_idx]
            parts      = label.split("_", 1)
            scientific = parts[0]
            common     = parts[1] if len(parts) > 1 else parts[0]

            detections.append({
                "species":    common,
                "scientific": scientific,
                "confidence": round(conf, 4),
                "time_s":     float(time_s),
            })

    # Output results as JSON to stdout (read by the VAMP plugin via popen)
    print(json.dumps(detections), flush=True)


if __name__ == "__main__":
    main()
