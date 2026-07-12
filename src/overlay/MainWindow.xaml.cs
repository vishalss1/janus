using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Threading;
using System.Windows.Controls;

namespace GestureOverlay
{
    public partial class MainWindow : Window
    {
        // P/Invoke for click-through transparent window styles
        private const int GWL_EXSTYLE = -20;
        private const int WS_EX_TRANSPARENT = 0x00000020;
        private const int WS_EX_LAYERED = 0x00080000;

        [DllImport("user32.dll", SetLastError = true)]
        private static extern int GetWindowLong(IntPtr hWnd, int nIndex);

        [DllImport("user32.dll")]
        private static extern int SetWindowLong(IntPtr hWnd, int nIndex, int dwNewLong);

        private SharedMemoryReader? _shmReader;
        private HandRenderer? _handRenderer;
        private CalibrationManager? _calibrationManager;
        private DispatcherTimer? _timer;
        private bool _isConnected = false;
        private int _reconnectAttempts = 0;

        public MainWindow()
        {
            InitializeComponent();
        }

        private void Window_Loaded(object sender, RoutedEventArgs e)
        {
            // Set window styles to enable layered transparency and click-through by default
            SetClickThrough(true);

            _shmReader = new SharedMemoryReader();
            _handRenderer = new HandRenderer(HandCanvas);
            
            _calibrationManager = new CalibrationManager(
                CalibrationCanvas,
                CalibrationTarget,
                CalibrationTargetCenter,
                CalibrationText,
                SetClickThrough,
                UpdateCalibrationStatus
            );

            // Periodically check calibration profile file status on startup
            string profilePath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "calibration_profile.json");
            if (File.Exists(profilePath))
            {
                CalibrationProfileStatus.Text = "CALIBRATED (LOADED)";
                CalibrationProfileStatus.Foreground = new SolidColorBrush(Color.FromRgb(46, 204, 113)); // Green
            }

            // Polling timer (60 Hz for fluid skeletal rendering)
            _timer = new DispatcherTimer(DispatcherPriority.Render);
            _timer.Interval = TimeSpan.FromMilliseconds(16);
            _timer.Tick += ProcessFrameTick;
            _timer.Start();
        }

        private void Window_Closing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            _timer?.Stop();
            _shmReader?.Dispose();
        }

        private void SetClickThrough(bool transparent)
        {
            var hwnd = new WindowInteropHelper(this).Handle;
            if (hwnd == IntPtr.Zero) return;

            int extendedStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
            if (transparent)
            {
                // Force input events to pass through the window
                SetWindowLong(hwnd, GWL_EXSTYLE, extendedStyle | WS_EX_TRANSPARENT | WS_EX_LAYERED);
            }
            else
            {
                // Restore input interception (so click actions on button/targets work)
                SetWindowLong(hwnd, GWL_EXSTYLE, extendedStyle & ~WS_EX_TRANSPARENT);
            }
        }

        private void ProcessFrameTick(object? sender, EventArgs e)
        {
            if (_shmReader == null) return;

            if (!_isConnected)
            {
                _reconnectAttempts++;
                if (_reconnectAttempts % 30 == 0) // Try connecting every 0.5s
                {
                    if (_shmReader.Open())
                    {
                        _isConnected = true;
                        ConnectionLed.Fill = new SolidColorBrush(Color.FromRgb(46, 204, 113)); // Green
                        ConnectionStateText.Text = "CONNECTED";
                        ConnectionStateText.Foreground = new SolidColorBrush(Color.FromRgb(46, 204, 113));
                    }
                }
                return;
            }

            unsafe
            {
                SharedMemoryLayout layout;
                if (_shmReader.Read(out layout))
                {
                    // Validate magic header "HAND" (0x48414E44)
                    if (layout.Magic != 0x48414E44)
                    {
                        DisconnectShm();
                        return;
                    }

                    // Render hand skeleton on canvas
                    _handRenderer?.Render(ref layout, HandCanvas.ActualWidth, HandCanvas.ActualHeight);

                    // Update gaze cursor blob position
                    double actualWidth = CursorCanvas.ActualWidth > 0 ? CursorCanvas.ActualWidth : SystemParameters.PrimaryScreenWidth;
                    double actualHeight = CursorCanvas.ActualHeight > 0 ? CursorCanvas.ActualHeight : SystemParameters.PrimaryScreenHeight;
                    
                    // Gaze coordinates are normalized (0 to 1) in screen-space, mirror x to align with layout
                    Canvas.SetLeft(GazeCursor, (1.0f - layout.CursorX) * actualWidth);
                    Canvas.SetTop(GazeCursor, layout.CursorY * actualHeight);

                    // Show active state label in HUD
                    UpdateStateText(layout.ActiveState);

                    // Update swipe gesture visual alert
                    UpdateSwipeIndicator(layout.SwipeDirection);

                    // Run calibration tick if active
                    _calibrationManager?.UpdateFrame(ref layout);
                }
                else
                {
                    DisconnectShm();
                }
            }
        }

        private void DisconnectShm()
        {
            _isConnected = false;
            _shmReader?.Close();
            ConnectionLed.Fill = new SolidColorBrush(Color.FromRgb(255, 59, 48)); // Red
            ConnectionStateText.Text = "DISCONNECTED";
            ConnectionStateText.Foreground = new SolidColorBrush(Color.FromRgb(255, 59, 48));
            StateText.Text = "OFFLINE";
            StateText.Foreground = new SolidColorBrush(Color.FromRgb(128, 128, 128));
            HandCanvas.Children.Clear();
        }

        private void UpdateStateText(uint activeState)
        {
            // HOVER=0, PINCH=1, FIST=2, SWIPE=3, ZOOM=4, POINT=5
            string stateStr;
            Color stateColor;

            switch (activeState)
            {
                case 0:
                    stateStr = "HOVER";
                    stateColor = Color.FromRgb(0, 240, 255); // Cyan
                    break;
                case 1:
                    stateStr = "PINCH (CLICK)";
                    stateColor = Color.FromRgb(255, 138, 0); // Orange
                    break;
                case 2:
                    stateStr = "FIST (GRAB)";
                    stateColor = Color.FromRgb(255, 59, 48); // Red
                    break;
                case 3:
                    stateStr = "SWIPE (SCROLL)";
                    stateColor = Color.FromRgb(139, 0, 255); // Purple
                    break;
                case 4:
                    stateStr = "ZOOM";
                    stateColor = Color.FromRgb(46, 204, 113); // Green
                    break;
                case 5:
                    stateStr = "POINT";
                    stateColor = Color.FromRgb(255, 235, 59); // Yellow
                    break;
                default:
                    stateStr = "UNKNOWN";
                    stateColor = Colors.White;
                    break;
            }

            StateText.Text = stateStr;
            StateText.Foreground = new SolidColorBrush(stateColor);
        }

        private void UpdateSwipeIndicator(uint swipeDirection)
        {
            // NONE=0, LEFT=1, RIGHT=2, UP=4, DOWN=8
            if (swipeDirection == 0) return;

            string directionStr = "";
            switch (swipeDirection)
            {
                case 1: directionStr = "◀ SWIPE LEFT"; break;
                case 2: directionStr = "SWIPE RIGHT ▶"; break;
                case 4: directionStr = "▲ SWIPE UP"; break;
                case 8: directionStr = "▼ SWIPE DOWN"; break;
            }

            SwipeAlertText.Text = directionStr;
            SwipeAlertText.Opacity = 1.0;

            // Simple fading effect
            var timer = new DispatcherTimer();
            timer.Interval = TimeSpan.FromMilliseconds(50);
            timer.Tick += (s, args) =>
            {
                SwipeAlertText.Opacity -= 0.08;
                if (SwipeAlertText.Opacity <= 0)
                {
                    SwipeAlertText.Opacity = 0;
                    timer.Stop();
                }
            };
            timer.Start();
        }

        private void UpdateCalibrationStatus(string status)
        {
            CalibrationProfileStatus.Text = status;
            if (status.StartsWith("ACTIVE"))
            {
                CalibrationProfileStatus.Foreground = new SolidColorBrush(Color.FromRgb(46, 204, 113)); // Green
            }
            else if (status.StartsWith("IN PROGRESS"))
            {
                CalibrationProfileStatus.Foreground = new SolidColorBrush(Color.FromRgb(255, 138, 0)); // Orange
            }
            else
            {
                CalibrationProfileStatus.Foreground = new SolidColorBrush(Color.FromRgb(255, 59, 48)); // Red
            }
        }

        private void CalibrateButton_Click(object sender, RoutedEventArgs e)
        {
            if (_calibrationManager == null) return;
            
            if (_calibrationManager.IsActive) return;

            _calibrationManager.Start();
        }
    }
}
