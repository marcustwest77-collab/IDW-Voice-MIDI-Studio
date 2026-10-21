#include "VoiceTrackNumber.h"
#include <stdexcept>
#include <string>
int main(){
    using idw::daw::voiceTrackNumber;
    for(int n=1;n<=128;++n)
        if(voiceTrackNumber(std::to_string(n))!=n)throw std::runtime_error("numeric track");
    if(voiceTrackNumber("three")!=3 || voiceTrackNumber(" Twenty-One ")!=21 ||
       voiceTrackNumber("ninety nine")!=99 || voiceTrackNumber("one hundred")!=100 ||
       voiceTrackNumber("one hundred and twenty eight")!=128 ||
       voiceTrackNumber("one hundred twenty eight")!=128)
        throw std::runtime_error("spoken track");
    for(const auto* invalid:{"", "zero", "0", "129", "999999999999999999999", "-1",
            "one hundred twenty nine", "three please", "3.5", "third", "two tracks"})
        if(voiceTrackNumber(invalid)!=0)throw std::runtime_error("invalid track phrase");
}
