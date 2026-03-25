#!/usr/bin/env python3
"""
birdnet_run.py — BirdNET inference script for the Audacity VAMP plugin.

This script is called by the VAMP plugin (BirdNetPlugin.cpp) as a subprocess.
It loads a BirdNET acoustic model, runs species prediction on a WAV file,
and prints the results as a JSON array to stdout.

Consecutive or overlapping detections of the same species are merged into a
single detection spanning from the first to the last segment, with confidence
computed as the average across all merged segments.

Usage:
    python3 birdnet_run.py <wav_path> [threshold] [top_k] [stride]

Arguments:
    wav_path   : Path to the input WAV file.
    threshold  : Minimum confidence score to report a detection (default: 0.5).
    top_k      : Maximum number of species to consider per segment (default: 3).
    stride     : Sliding window step in seconds, in range [0.1, 3.0] (default: 3.0).

Output:
    JSON array of detections, each containing:
        - species    : Common name of the detected species.
        - scientific : Scientific name of the detected species.
        - confidence : Average confidence score across merged segments (4 decimal places).
        - time_s     : Start time of the merged detection in seconds.
        - end_s      : End time of the merged detection in seconds.
"""

import sys
import json
import birdnet
import os
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '3'  # suppress TensorFlow logs

def merge_detections(detections):
    """
    Merge consecutive or overlapping detections of the same species.

    Two or more detections of the same species are merged if their start times
    are <= the end time of the current accumulated segment (i.e., they overlap
    or are exactly consecutive). The merged detection spans from the first
    start to the last end, and its confidence is the average of all merged
    segments.

    Args:
        detections : List of detection dicts sorted by time_s.

    Returns:
        List of merged detection dicts.
    """
    if not detections:
        return []

    # Sort by species then by start time for consistent merging
    detections.sort(key=lambda d: (d["species"], d["time_s"]))

    merged = []
    current = dict(detections[0])
    current["_conf_sum"]     = current["confidence"]
    current["_conf_count"]   = 1

    for det in detections[1:]:
        det_end = det["end_s"]
        same_species = det["species"] == current["species"]
        overlapping  = det["time_s"] <= current["end_s"]

        if same_species and overlapping:
            # Extend current segment and accumulate confidence
            current["end_s"]       = max(current["end_s"], det_end)
            current["_conf_sum"]  += det["confidence"]
            current["_conf_count"] += 1
        else:
            # Finalise current segment and start a new one
            current["confidence"] = round(current["_conf_sum"] / current["_conf_count"], 4)
            del current["_conf_sum"], current["_conf_count"]
            merged.append(current)
            current = dict(det)
            current["end_s"]       = det_end
            current["_conf_sum"]   = det["confidence"]
            current["_conf_count"] = 1

    # Finalise last segment
    current["confidence"] = round(current["_conf_sum"] / current["_conf_count"], 4)
    del current["_conf_sum"], current["_conf_count"]
    merged.append(current)

    # Re-sort by start time for output
    merged.sort(key=lambda d: d["time_s"])
    return merged


def main():
    # Parse command-line arguments
    wav_path  = sys.argv[1]
    threshold = float(sys.argv[2]) if len(sys.argv) > 2 else 0.5
    top_k     = int(sys.argv[3])   if len(sys.argv) > 3 else 3
    stride    = float(sys.argv[4]) if len(sys.argv) > 4 else 3.0

    # Clamp stride to valid range and compute overlap
    stride  = max(0.1, min(3.0, stride))  # ensure stride is in [0.1, 3.0]
    overlap = max(0.0, 3.0 - stride)      # overlap = window_duration - stride

    lat, lon = 42.5, -76.45
    week = 4

    # 1. filtra espécies por localização e semana
    geo_model = birdnet.load("geo", "2.4", "tf")
    geo_result = geo_model.predict(lat, lon, week=week, min_confidence=0.03)
    species_filter = geo_result.to_set()

    species_filter=None
    
    # Load BirdNET acoustic model v2.4 with TensorFlow backend
    model = birdnet.load("acoustic", "2.4", "tf")

    # Run prediction with sliding window
    result = model.predict( 
        wav_path,
        default_confidence_threshold=threshold,
        top_k=top_k,
        overlap_duration_s=overlap,
        custom_species_list=species_filter
    )

    data = result.to_structured_array()
    detections = []
    
    for row in data:
        species    = row['species_name']
        scientific = species.split("_")[0]
        common     = species.split("_")[1]
        conf       = float(row['confidence'])
        
        detections.append({
            "species":    common,
            "scientific": scientific,
            "confidence": round(conf, 4),
            "time_s":     float(row['start_time']),
            "end_s":      float(row['end_time']),
        })

    # Merge consecutive/overlapping detections of the same species
    detections = merge_detections(detections)

    # Output results as JSON to stdout (read by the VAMP plugin via popen)
    print(json.dumps(detections), flush=True)


if __name__ == "__main__":
    main()
