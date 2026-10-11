#include "DictationAudioQueue.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
    idw::DictationAudioQueue queue;short result[8]{};
    const float left[]{.25f,2.f,std::numeric_limits<float>::quiet_NaN(),-.5f};
    const float right[]{-.25f,-2.f,.5f,.5f};
    check(queue.push(left,right,4,0)&&queue.pop(result,8)==4,"Left capture failed");
    check(result[0]==8191&&result[1]==32767&&result[2]==0&&result[3]==-16383,"PCM conversion/clipping failed");
    check(queue.push(left,right,4,1)&&queue.pop(result,8)==4&&result[0]==-8191,"Right input failed");
    check(queue.push(left,right,4,2)&&queue.pop(result,8)==4&&result[0]==0&&result[3]==0,"Mixed input failed");
    check(queue.push(left,nullptr,4,1)&&queue.pop(result,8)==4&&result[0]==8191,"Mono fallback failed");
    std::vector<float> full(idw::DictationAudioQueue::capacity,.25f);
    check(queue.push(full.data(),nullptr,(int)full.size(),0),"Queue capacity failed");
    check(!queue.push(left,nullptr,1,0),"Overflow overwrote unread samples");
    std::vector<short> drain(full.size());check(queue.pop(drain.data(),(int)drain.size())==(int)full.size(),"Drain failed");
    for(auto s:drain)check(s==8191,"Wraparound corrupted PCM");
    constexpr int total=1000000;std::atomic<bool> failed{false};
    std::thread producer([&]{for(int i=0;i<total;++i){const float x=(i%2)?-.25f:.25f;while(!queue.push(&x,nullptr,1,0))std::this_thread::yield();}});
    int read=0;while(read<total){short block[257];const int count=queue.pop(block,257);for(int i=0;i<count;++i,++read)if(block[i]!=((read%2)?-8191:8191))failed=true;}
    producer.join();check(!failed,"Concurrent PCM order failed");
    std::cout<<"PASS dictation PCM selection, clipping, overflow, wraparound and concurrent ordering\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
