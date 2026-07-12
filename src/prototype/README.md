# Phase 0 — Python Prototype

This prototype provides a fast, high-level way to validate gesture heuristics, state transition thresholds, and FSM debounce values before committing to C++ development.

## Setup Instructions

1. Ensure you have Python 3.9+ installed.
2. Create and activate a virtual environment:
   ```bash
   python -m venv .venv
   .venv\Scripts\activate
   ```
3. Install required packages:
   ```bash
   pip install -r requirements.txt
   ```

## Running the Prototype

1. Connect an RGB webcam.
2. Run the script:
   ```bash
   python prototype.py
   ```
3. Controls:
   - Move your hand in front of the camera. The cursor will follow your index finger tip.
   - Bring your index finger and thumb together (pinch) to initiate a left click and drag.
   - Press **ESC** on your keyboard to exit the application.

## Tuning Targets

Before starting Phase 1, you should adjust the following parameters inside `prototype.py` to match your personal setup:
- `self.debounce_threshold_ms` - Hysteresis buffer time to prevent raw frame jitter.
- Pinch distance threshold (currently set to `0.05` normalized space).
