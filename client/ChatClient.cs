using System;
using System.Threading.Tasks;
using ConsoleApplication;

namespace ConsoleApplication
{
    class ChatClient : IWSClient
    {
        WsClient _wsclient;
        public ChatClient()
        {
            _wsclient = new WsClient(this);
        }

        public Task ConnectTo(string wsurl)
        {
            return _wsclient.ConnectAsync(wsurl);
        }

        public void OnClose()
        {
            Console.WriteLine("OnClose");
        }

        public void OnConnect()
        {
            Console.WriteLine("OnConnect");
            //Task.Run(() => _wsclient.StartMessageLoopAsync());

            while (true)
            {
                var message = Console.ReadLine();
                if (message == "Bye") {
                    return;
                }

                _wsclient.SendMessageAsync(message);
            }
        }

        public void OnError(string error)
        {
            Console.WriteLine("OnError:" + error);
        }

        public void OnMessage(string message)
        {
            Console.WriteLine("OnMessage: " + message);
        }

    }
}
