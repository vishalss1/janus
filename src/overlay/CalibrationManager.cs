using System;
using System.Collections.Generic;
using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Threading;

namespace GestureOverlay
{
    public class CalibrationPoint
    {
        public double TargetX { get; set; }
        public double TargetY { get; set; }
        public double GazeX { get; set; }
        public double GazeY { get; set; }
    }

    public class CalibrationManager
    {
        private readonly Canvas _canvas;
        private readonly UIElement _targetCircle;
        private readonly UIElement _targetCenter;
        private readonly TextBlock _textBlock;
        private readonly Action<bool> _setClickThrough;
        private readonly Action<string> _updateStatus;
        
        private readonly List<Point> _targetPoints = new List<Point>
        {
            new Point(0.5, 0.5),   // Center
            new Point(0.1, 0.1),   // Top-Left
            new Point(0.9, 0.1),   // Top-Right
            new Point(0.1, 0.9),   // Bottom-Left
            new Point(0.9, 0.9)    // Bottom-Right
        };

        private int _currentPointIndex = -1;
        private readonly List<CalibrationPoint> _calibrationData = new List<CalibrationPoint>();
        
        private bool _isCapturing = false;
        private DateTime _captureStartTime;
        private readonly List<Point> _samples = new List<Point>();

        public bool IsActive => _currentPointIndex >= 0;

        public CalibrationManager(
            Canvas canvas, 
            UIElement targetCircle, 
            UIElement targetCenter, 
            TextBlock textBlock,
            Action<bool> setClickThrough,
            Action<string> updateStatus)
        {
            _canvas = canvas;
            _targetCircle = targetCircle;
            _targetCenter = targetCenter;
            _textBlock = textBlock;
            _setClickThrough = setClickThrough;
            _updateStatus = updateStatus;
        }

        public void Start()
        {
            _calibrationData.Clear();
            _currentPointIndex = 0;
            _setClickThrough(false); // Disable click-through so user knows they are calibrating
            _canvas.Visibility = Visibility.Visible;
            _updateStatus("IN PROGRESS...");
            ShowNextPoint();
        }

        private void ShowNextPoint()
        {
            if (_currentPointIndex >= _targetPoints.Count)
            {
                Finish();
                return;
            }

            var target = _targetPoints[_currentPointIndex];
            double actualWidth = _canvas.ActualWidth > 0 ? _canvas.ActualWidth : SystemParameters.PrimaryScreenWidth;
            double actualHeight = _canvas.ActualHeight > 0 ? _canvas.ActualHeight : SystemParameters.PrimaryScreenHeight;

            double px = target.X * actualWidth;
            double py = target.Y * actualHeight;

            // Update target shapes positions
            Canvas.SetLeft(_targetCircle, px);
            Canvas.SetTop(_targetCircle, py);
            Canvas.SetLeft(_targetCenter, px);
            Canvas.SetTop(_targetCenter, py);

            // Update instruction text layout
            _textBlock.Text = $"Look here and PINCH\n(Point {_currentPointIndex + 1} of {_targetPoints.Count})";
            Canvas.SetLeft(_textBlock, px - 80);
            Canvas.SetTop(_textBlock, py + 30);

            _isCapturing = false;
            _samples.Clear();
        }

        public void UpdateFrame(ref SharedMemoryLayout layout)
        {
            if (!IsActive) return;

            // Trigger capture when user makes a Pinch gesture
            bool gestureDetected = layout.PinchActive > 0;
            
            if (gestureDetected)
            {
                if (!_isCapturing)
                {
                    _isCapturing = true;
                    _captureStartTime = DateTime.Now;
                }

                // Sample current gaze positions
                _samples.Add(new Point(layout.CursorX, layout.CursorY));

                // Hold gesture for 1 second to gather enough samples
                if ((DateTime.Now - _captureStartTime).TotalSeconds >= 1.0)
                {
                    double avgX = 0, avgY = 0;
                    foreach (var sample in _samples)
                    {
                        avgX += sample.X;
                        avgY += sample.Y;
                    }
                    avgX /= _samples.Count;
                    avgY /= _samples.Count;

                    var currentTarget = _targetPoints[_currentPointIndex];
                    _calibrationData.Add(new CalibrationPoint
                    {
                        TargetX = currentTarget.X,
                        TargetY = currentTarget.Y,
                        GazeX = avgX,
                        GazeY = avgY
                    });

                    _currentPointIndex++;
                    ShowNextPoint();
                }
            }
            else
            {
                // Reset capturing if they release early
                _isCapturing = false;
                _samples.Clear();
            }
        }

        private void Finish()
        {
            _canvas.Visibility = Visibility.Collapsed;
            _currentPointIndex = -1;
            _setClickThrough(true); // Re-enable click-through

            // Save profile to json in workspace root
            string filePath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "calibration_profile.json");
            
            try
            {
                using (var writer = new StreamWriter(filePath))
                {
                    writer.WriteLine("{");
                    writer.WriteLine("  \"points\": [");
                    for (int i = 0; i < _calibrationData.Count; i++)
                    {
                        var pt = _calibrationData[i];
                        string comma = i < _calibrationData.Count - 1 ? "," : "";
                        writer.WriteLine($"    {{ \"target_x\": {pt.TargetX:F3}, \"target_y\": {pt.TargetY:F3}, \"gaze_x\": {pt.GazeX:F3}, \"gaze_y\": {pt.GazeY:F3} }}{comma}");
                    }
                    writer.WriteLine("  ]");
                    writer.WriteLine("}");
                }
                _updateStatus("ACTIVE (CALIBRATED)");
                MessageBox.Show("Calibration completed successfully! Gaze mapping saved to calibration_profile.json.", "Janus Calibration", MessageBoxButton.OK, MessageBoxImage.Information);
            }
            catch (Exception ex)
            {
                _updateStatus("SAVE ERROR");
                MessageBox.Show($"Failed to save calibration profile: {ex.Message}", "Calibration Error", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }
    }
}
