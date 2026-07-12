using System;
using System.IO;
using System.IO.MemoryMappedFiles;
using System.Runtime.InteropServices;

namespace GestureOverlay
{
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct Landmark
    {
        public float X;
        public float Y;
        public float Z;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 4)]
    public unsafe struct SharedMemoryLayout
    {
        public uint Magic;
        public uint LayoutVersion;
        public uint SequenceNumber;

        public byte HandDetected;
        public byte PinchActive;
        private fixed byte _padding[2];

        public uint ActiveState;
        public uint SwipeDirection;

        public float CursorX;
        public float CursorY;

        public fixed float Landmarks[21 * 3]; // 21 landmarks * 3 floats (x, y, z) = 63 floats
    }

    public class SharedMemoryReader : IDisposable
    {
        private const string SharedMemoryName = "Local\\GestureInputDeviceSharedMemory";
        private MemoryMappedFile? _mmf;
        private MemoryMappedViewAccessor? _accessor;
        private unsafe byte* _pointer;

        public bool Open()
        {
            try
            {
                _mmf = MemoryMappedFile.OpenExisting(SharedMemoryName, MemoryMappedFileRights.Read);
                _accessor = _mmf.CreateViewAccessor(0, 0, MemoryMappedFileAccess.Read);
                
                unsafe
                {
                    _accessor.SafeMemoryMappedViewHandle.AcquirePointer(ref _pointer);
                }
                return true;
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"SharedMemoryReader open failed: {ex.Message}");
                Close();
                return false;
            }
        }

        public unsafe bool Read(out SharedMemoryLayout layout)
        {
            layout = default;
            if (_pointer == null || _accessor == null)
            {
                return false;
            }

            try
            {
                layout = *(SharedMemoryLayout*)_pointer;
                return true;
            }
            catch (Exception)
            {
                return false;
            }
        }

        public unsafe void Close()
        {
            if (_accessor != null)
            {
                if (_pointer != null)
                {
                    _accessor.SafeMemoryMappedViewHandle.ReleasePointer();
                    _pointer = null;
                }
                _accessor.Dispose();
                _accessor = null;
            }
            if (_mmf != null)
            {
                _mmf.Dispose();
                _mmf = null;
            }
        }

        public void Dispose()
        {
            Close();
        }
    }
}
