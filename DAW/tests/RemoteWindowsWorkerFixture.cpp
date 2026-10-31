#include "../RemoteNativeLocalReceiver.h"
#include "../RemoteWindowsWorkerProcess.h"
#include <iostream>
int main(int argc,char** argv) {
#ifdef _WIN32
    using Worker=idw::daw::remote::WindowsWorkerProcess;
    if (argc!=4) return 2;
    std::string json;
    if (!std::getline(std::cin,json)) return 3;
    Worker worker;
    const auto first=Worker::freshSecret(),second=Worker::freshSecret();
    if (first.size()!=64 || second.size()!=64 || first==second) return 4;
    const auto python=std::filesystem::path(argv[1]).wstring();
    const auto script=std::filesystem::path(argv[2]).wstring();
    if (!worker.start(python,script,json)) { std::cout<<"REJECTED\n";return 0; }
    const bool stopReady=std::string(argv[3])=="stop-ready";
    bool notifyReady=std::string(argv[3])=="notify-ready";
    while (true) {
        const auto state=worker.poll();
        if (state==Worker::State::ready && notifyReady) {
            std::cout<<"READY"<<std::endl;
            notifyReady=false;
        }
        if (state==Worker::State::ready && stopReady) {
            worker.stop(); std::cout<<"STOPPED\n"; return 0;
        }
        if (state==Worker::State::succeeded || state==Worker::State::failed) {
            std::cout<<(state==Worker::State::succeeded ? "SUCCESS\n" : "FAILED\n");
            return 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
#else
    (void)argc;(void)argv;
    std::cout<<"UNSUPPORTED\n";
#endif
}
