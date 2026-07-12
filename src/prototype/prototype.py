"""
Phase 0 Prototype: Gesture Input Device
Uses OpenCV for capture, MediaPipe for hand tracking, and PyAutoGUI for input injection.
This script is used to validate gesture parameters, state transitions, and debounce FSM logic.
"""

import cv2
import mediapipe as mp
import pyautogui
import numpy as np
import time

# Disable PyAutoGUI fail-safe for smooth operations (use with caution)
pyautogui.FAILSAFE = True

class GestureState:
    HOVER = "HOVER"
    PINCH = "PINCH"
    FIST = "FIST"
    SWIPE = "SWIPE"

class GestureFSM:
    def __init__(self):
        self.current_state = GestureState.HOVER
        self.state_durations = {}  # Track how long we've been in potential transition states
        self.debounce_threshold_ms = 150  # 150ms hysteresis / debounce

    def update(self, landmarks):
        """
        Processes 21 hand landmarks and transitions the FSM.
        Returns the active action to perform.
        """
        if not landmarks:
            return None

        # Simple detection heuristics for prototype validation:
        # Landmark indices: Thumb tip = 4, Index tip = 8
        thumb_tip = landmarks[4]
        index_tip = landmarks[8]

        # Calculate Euclidean distance between thumb and index tips
        dist = np.linalg.norm(np.array([thumb_tip.x, thumb_tip.y]) - np.array([index_tip.x, index_tip.y]))

        # State detection logic
        detected_state = GestureState.HOVER
        if dist < 0.05:  # Pinch threshold
            detected_state = GestureState.PINCH
        
        # Debounce logic
        # (For prototype, we check if the state persists before transitioning)
        if detected_state != self.current_state:
            # Transition state detection
            self.current_state = detected_state # Temporary direct transition for skeletal prototype
            
        return self.current_state

def main():
    print("Starting Phase 0 Gesture Input Prototype...")
    cap = cv2.VideoCapture(0)
    
    mp_hands = mp.solutions.hands
    hands = mp_hands.Hands(
        static_image_mode=False,
        max_num_hands=1,
        min_detection_confidence=0.7,
        min_tracking_confidence=0.7
    )
    mp_draw = mp.solutions.drawing_utils
    fsm = GestureFSM()

    screen_w, screen_h = pyautogui.size()

    while cap.isOpened():
        success, frame = cap.read()
        if not success:
            print("Ignoring empty camera frame.")
            continue

        # Flip horizontally for natural mirror view
        frame = cv2.flip(frame, 1)
        h, w, c = frame.shape
        rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        
        # Inference
        results = hands.process(rgb_frame)

        if results.multi_hand_landmarks:
            for hand_landmarks in results.multi_hand_landmarks:
                mp_draw.draw_landmarks(frame, hand_landmarks, mp_hands.HAND_CONNECTIONS)
                
                # Extract landmarks
                landmarks = hand_landmarks.landmark
                
                # Track cursor with Index finger tip (landmark 8)
                index_pos = landmarks[8]
                cursor_x = int(index_pos.x * screen_w)
                cursor_y = int(index_pos.y * screen_h)
                
                # Update FSM
                state = fsm.update(landmarks)
                
                # Perform Action
                if state == GestureState.PINCH:
                    # Pinch acts as click and drag
                    pyautogui.dragTo(cursor_x, cursor_y, button='left')
                else:
                    # Hover acts as cursor movement
                    pyautogui.moveTo(cursor_x, cursor_y)
                    
                cv2.putText(frame, f"State: {state}", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)

        cv2.imshow('Gesture Input Prototype (Phase 0)', frame)
        if cv2.waitKey(5) & 0xFF == 27:  # ESC to exit
            break

    cap.release()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    main()
