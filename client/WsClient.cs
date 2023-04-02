
using System;
using System.Linq;
using System.Net.WebSockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace ConsoleApplication
{
    public class WsClient
    {
        private ClientWebSocket _wsocket;
        private CancellationTokenSource _cts;
	    private IWSClient _client;
	    public bool IsConnected { get; set;} 
        public WsClient(IWSClient client)
        {
            IsConnected = false;
            _wsocket = new ClientWebSocket();
            _cts = new CancellationTokenSource();
            _client = client;
        }
        public async Task ConnectAsync(string wsUri)
        {
            var connectTask = _wsocket.ConnectAsync(new Uri(wsUri), _cts.Token);

            try
            {
                connectTask.Wait();
                if (connectTask.IsFaulted)
                {
                    _client.OnClose();
                }
                else
                {
                    await StartMessageLoopAsync();
                    _client.OnConnect();
                }
            }
            catch (AggregateException ae)
            {
                ae.Handle((x) =>
                {
                    _client.OnError(x.Message);
                    return true;
                });
            }
        }
        public  Task StartMessageLoopAsync()
        {
            return Task.Factory.StartNew(async () =>
            {
                Console.WriteLine("task receive");
                var rcvBuffer = new ArraySegment<byte>(new byte[128]);
                while (_wsocket.State == WebSocketState.Open)
                {
                    WebSocketReceiveResult rcvResult = await _wsocket.ReceiveAsync(rcvBuffer, _cts.Token);
                    byte[] msgBytes = rcvBuffer.Skip(rcvBuffer.Offset).Take(rcvResult.Count).ToArray();
                    string rcvMsg = Encoding.UTF8.GetString(msgBytes);
                    _client.OnMessage(rcvMsg);
                }
            }, _cts.Token, TaskCreationOptions.LongRunning, TaskScheduler.Default);
        }

        private void  Cancel() {
            _cts.Cancel();
        }
        public async void SendMessageAsync(string message)
        {
            byte[] sendBytes = Encoding.UTF8.GetBytes(message);
            var sendBuffer = new ArraySegment<byte>(sendBytes);
            await _wsocket.SendAsync(
                sendBuffer,
                WebSocketMessageType.Text,
                endOfMessage: true,
                cancellationToken: _cts.Token);
        }
    }
}