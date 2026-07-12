using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Shapes;
using System.Windows;
using System.Windows.Media.Effects;

namespace GestureOverlay
{
    public class HandRenderer
    {
        private readonly Canvas _canvas;
        private readonly SolidColorBrush _jointBrush = new SolidColorBrush(Color.FromRgb(0, 240, 255));
        private readonly SolidColorBrush _boneBrush = new SolidColorBrush(Color.FromArgb(120, 255, 255, 255));
        private readonly SolidColorBrush _pinchBrush = new SolidColorBrush(Color.FromRgb(255, 138, 0));

        // Joint connections matching MediaPipe Hand topology:
        // Wrist (0), Thumb (1-4), Index (5-8), Middle (9-12), Ring (13-16), Pinky (17-20)
        private static readonly int[][] Bones = new int[][]
        {
            new int[] { 0, 1 }, new int[] { 1, 2 }, new int[] { 2, 3 }, new int[] { 3, 4 },     // Thumb
            new int[] { 0, 5 }, new int[] { 5, 6 }, new int[] { 6, 7 }, new int[] { 7, 8 },     // Index
            new int[] { 5, 9 }, new int[] { 9, 10 }, new int[] { 10, 11 }, new int[] { 11, 12 }, // Middle
            new int[] { 9, 13 }, new int[] { 13, 14 }, new int[] { 14, 15 }, new int[] { 15, 16 }, // Ring
            new int[] { 0, 17 }, new int[] { 17, 18 }, new int[] { 18, 19 }, new int[] { 19, 20 }, // Pinky
            new int[] { 13, 17 } // Base palm connection
        };

        public HandRenderer(Canvas canvas)
        {
            _canvas = canvas;
        }

        public unsafe void Render(ref SharedMemoryLayout layout, double width, double height)
        {
            _canvas.Children.Clear();

            if (layout.HandDetected == 0)
            {
                return;
            }

            // Extract joint screen coordinates
            var points = new Point[21];
            fixed (float* landmarks = layout.Landmarks)
            {
                for (int i = 0; i < 21; ++i)
                {
                    // MediaPipe landmarks are normalized (0 to 1) relative to frame size.
                    // X coordinates might need to be mirrored depending on webcam orientation.
                    // The camera is usually mirrored, so 1.0 - X reflects it to match hand position.
                    float x = landmarks[i * 3 + 0];
                    float y = landmarks[i * 3 + 1];

                    points[i] = new Point((1.0f - x) * width, y * height);
                }
            }

            // 1. Draw bones (skeletal lines)
            foreach (var bone in Bones)
            {
                var p1 = points[bone[0]];
                var p2 = points[bone[1]];

                var line = new Line
                {
                    X1 = p1.X,
                    Y1 = p1.Y,
                    X2 = p2.X,
                    Y2 = p2.Y,
                    Stroke = _boneBrush,
                    StrokeThickness = 2.5
                };
                _canvas.Children.Add(line);
            }

            // 2. Draw joints (circles)
            bool isPinching = layout.PinchActive > 0;
            var currentJointBrush = isPinching ? _pinchBrush : _jointBrush;

            for (int i = 0; i < 21; ++i)
            {
                var pt = points[i];
                double radius = (i == 4 || i == 8 || i == 12 || i == 16 || i == 20) ? 7.0 : 4.5;

                var ellipse = new Ellipse
                {
                    Width = radius * 2,
                    Height = radius * 2,
                    Fill = currentJointBrush
                };

                // Add drop shadow glow to key tip joints
                if (radius > 5)
                {
                    ellipse.Effect = new DropShadowEffect
                    {
                        BlurRadius = 10,
                        Color = isPinching ? Color.FromRgb(255, 138, 0) : Color.FromRgb(0, 240, 255),
                        ShadowDepth = 0,
                        Opacity = 0.8
                    };
                }

                Canvas.SetLeft(ellipse, pt.X - radius);
                Canvas.SetTop(ellipse, pt.Y - radius);
                _canvas.Children.Add(ellipse);
            }
        }
    }
}
