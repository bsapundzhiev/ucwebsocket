using System;
using System.Threading;
using System.Threading.Tasks;

namespace ConsoleApplication
{
    public class Program
    {
        public static void Main(string[] args)
        {
            Console.WriteLine("Init client");

            var chatClient = new ChatClient();
            chatClient.ConnectTo("ws://localhost:8088/echo").Wait();
        }

    }

}
