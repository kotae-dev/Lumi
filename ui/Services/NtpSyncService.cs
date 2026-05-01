using System;
using System.Net;
using System.Net.Sockets;
using System.Threading.Tasks;

namespace Lumi.Services
{
    /// <summary>
    /// Simple NTP sync client for time correction.
    /// Wraps the native NTP or provides a managed fallback.
    /// </summary>
    public class NtpSyncService
    {
        private static readonly Lazy<NtpSyncService> _lazy = new(() => new NtpSyncService());
        public static NtpSyncService Instance => _lazy.Value;

        public TimeSpan Offset { get; private set; } = TimeSpan.Zero;
        public bool IsSynced { get; private set; }

        private NtpSyncService() { }

        /// <summary>
        /// Sync with NTP server (managed fallback if native call fails).
        /// </summary>
        public async Task<bool> SyncAsync(string server = "pool.ntp.org")
        {
            try
            {
                return await Task.Run(() =>
                {
                    // Try native NTP sync first
                    try
                    {
                        int result = Core.NativeInterop.lumi_get_time() != 0 ? 0 : -1;
                        if (result == 0)
                        {
                            IsSynced = true;
                            return true;
                        }
                    }
                    catch
                    {
                        // Native call failed, use managed fallback
                    }

                    // Managed NTP fallback
                    try
                    {
                        var ntpData = new byte[48];
                        ntpData[0] = 0x1B; // LI=0, VN=3, Mode=3

                        var ep = new IPEndPoint(Dns.GetHostAddresses(server)[0], 123);
                        using var socket = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp);
                        socket.ReceiveTimeout = 3000;
                        socket.Connect(ep);

                        var localTime = DateTime.UtcNow;
                        socket.Send(ntpData);
                        socket.Receive(ntpData);

                        ulong intPart = (ulong)ntpData[40] << 24 | (ulong)ntpData[41] << 16 |
                                        (ulong)ntpData[42] << 8 | (ulong)ntpData[43];

                        var ntpTime = new DateTime(1900, 1, 1, 0, 0, 0, DateTimeKind.Utc)
                            .AddSeconds(intPart);

                        Offset = ntpTime - localTime;
                        IsSynced = true;
                        return true;
                    }
                    catch
                    {
                        return false;
                    }
                });
            }
            catch
            {
                return false;
            }
        }

        public DateTime GetCorrectedTime()
        {
            return DateTime.UtcNow + Offset;
        }
    }
}
