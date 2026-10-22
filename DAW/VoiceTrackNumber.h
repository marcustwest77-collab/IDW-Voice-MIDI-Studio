#pragma once
#include <string>
namespace idw::daw {
// Return a one-based track number, or zero for an unsupported phrase.
inline int voiceTrackNumber(const std::string& input) {
    const auto first=input.find_first_not_of(" \t");
    if(first!=std::string::npos && input[first]=='-')return 0;
    std::string text;
    for(unsigned char c:input){
        if(c==' ' || c=='-' || c=='\t'){
            if(!text.empty() && text.back()!=' ')text+=' ';
        }else text+=(c>='A' && c<='Z') ? char(c-'A'+'a') : char(c);
    }
    if(!text.empty() && text.back()==' ')text.pop_back();
    if(text.empty())return 0;
    int number=0;bool digits=true;
    for(char c:text){
        if(c<'0' || c>'9'){digits=false;break;}
        number=number*10+(c-'0');
        if(number>128)return 0;
    }
    if(digits)return number;
    const char* small[]={"", "one", "two", "three", "four", "five", "six", "seven",
        "eight", "nine", "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen",
        "sixteen", "seventeen", "eighteen", "nineteen"};
    const char* tens[]={"", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"};
    const auto belowHundred=[&](int value)->std::string {
        if(value<20)return small[value];
        return std::string(tens[value/10])+(value%10 ? " "+std::string(small[value%10]) : "");
    };
    for(int value=1;value<=128;++value){
        const auto tail=belowHundred(value%100);
        const auto words=value<100 ? belowHundred(value) :
            std::string("one hundred")+(value%100 ? " "+tail : "");
        if(text==words)return value;
        if(value>100 && text=="one hundred and "+tail)return value;
    }
    return 0;
}
}
