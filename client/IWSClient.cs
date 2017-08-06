namespace ConsoleApplication
{
    public interface IWSClient
    {
        void OnConnect();
        void OnMessage(string message);
        void OnClose();
        void OnError(string error);
    }
}
