// Contract fixture only. This does NOT recognize speech and is never packaged.
#include <chrono>
#include <thread>
#if defined(_WIN32)
#define API extern "C" __declspec(dllexport)
#else
#define API extern "C"
#endif
struct VoskModel{};
struct VoskRecognizer{int samples=0;};
API VoskModel* vosk_model_new(const char*){std::this_thread::sleep_for(std::chrono::milliseconds(80));return new VoskModel;}
API void vosk_model_free(VoskModel* model){delete model;}
API VoskRecognizer* vosk_recognizer_new(VoskModel*,float rate){return rate==48000?new VoskRecognizer:nullptr;}
API void vosk_recognizer_free(VoskRecognizer* recognizer){delete recognizer;}
API int vosk_recognizer_accept_waveform_s(VoskRecognizer* recognizer,const short* pcm,int count){
    for(int i=0;i<count;++i)if(pcm[i]!=8191)return -1;
    recognizer->samples+=count;return recognizer->samples>=480?1:0;
}
API const char* vosk_recognizer_result(VoskRecognizer*){return "{\"text\":\"contract phrase\"}";}
API const char* vosk_recognizer_partial_result(VoskRecognizer*){return "{\"partial\":\"contract\"}";}
API const char* vosk_recognizer_final_result(VoskRecognizer*){return "{\"text\":\"final phrase\"}";}
