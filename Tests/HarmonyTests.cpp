#include "HarmonyEngine.h"
#include <iostream>
#include <stdexcept>
void check(bool c){if(!c)throw std::runtime_error("Harmony regression");}
int main(){
 using namespace idw;
 auto c=harmonize(60,0,1,false,true);check(c.chord==std::array<int,4>{{60,64,67,-1}}&&c.bass==36);
 auto d=harmonize(62,0,1,true,true);check(d.chord==std::array<int,4>{{62,65,69,72}}&&d.bass==38);
 auto a=harmonize(69,9,2,true,false);check(a.chord==std::array<int,4>{{69,72,76,79}}&&a.bass==-1);
 check(harmonize(60,0,0,false,true)==HarmonyNotes{});check(harmonize(-1,0,1,false,true)==HarmonyNotes{});
 int count=0;
 for(int mode=1;mode<=2;++mode)for(int root=0;root<12;++root)for(int n=0;n<128;++n)for(bool seventh:{false,true}){
  const auto h=harmonize(n,root,mode,seventh,true);int previous=-1;
  for(int note:h.chord)if(note>=0){check(note<=127&&note>previous);previous=note;const int pc=(note-root+120)%12;check(mode==1?(pc==0||pc==2||pc==4||pc==5||pc==7||pc==9||pc==11):(pc==0||pc==2||pc==3||pc==5||pc==7||pc==8||pc==10));}
  check(h.bass>=0&&h.bass<=127);++count;
 }
 std::cout<<"PASS: chord examples, disabled/invalid input and "<<count<<" harmony range/scale cases\n";
}
