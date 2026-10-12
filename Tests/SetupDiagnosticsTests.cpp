#include "SetupDiagnostics.h"
#include <iostream>
#include <stdexcept>
#include <string>
int main(){using namespace idw;
 auto check=[](SetupState got,SetupState wanted){if(got!=wanted)throw std::runtime_error("Wrong setup guidance");};
 check(diagnose(false,.1f,.08f,.01f,false,true,false,.9f,.75f,true),SetupState::stopped);
 check(diagnose(true,1,.08f,.01f,false,true,false,.9f,.75f,true),SetupState::clipping);
 check(diagnose(true,.98f,.08f,.01f,false,true,false,.9f,.75f,true),SetupState::clipping);
 check(diagnose(true,.90f,.08f,.01f,false,true,false,.9f,.75f,true),SetupState::nearClipping);
 check(diagnose(true,.97f,.08f,.01f,false,true,false,.9f,.75f,false),SetupState::nearClipping);
 check(diagnose(true,.899f,.08f,.01f,false,true,false,.9f,.75f,true),SetupState::tracking);
 check(diagnose(true,0,0,.01f,false,true,false,0,.75f,false),SetupState::silent);
 check(diagnose(true,.1f,.08f,.01f,true,true,false,.9f,.75f,true),SetupState::training);
 check(diagnose(true,.93f,.08f,.01f,true,true,false,.9f,.75f,true),SetupState::training);
 check(diagnose(true,.1f,.08f,.01f,false,false,false,.9f,.75f,false),SetupState::disabled);
 check(diagnose(true,.01f,.005f,.01f,false,true,false,.9f,.75f,false),SetupState::belowGate);
 check(diagnose(true,.1f,.08f,.01f,false,true,false,.4f,.75f,false),SetupState::uncertain);
 check(diagnose(true,.1f,.08f,.01f,false,true,false,.9f,.75f,true),SetupState::tracking);
 check(diagnose(true,.1f,.005f,.01f,false,false,true,0,.75f,false),SetupState::listening);
 check(diagnose(true,.1f,.08f,.01f,false,true,false,.9f,.75f,false,false),SetupState::outsideRange);
 if(std::string(guidance(SetupState::nearClipping)).find("gain")==std::string::npos)
    throw std::runtime_error("Headroom guidance must advise gain adjustment");
 std::cout<<"PASS: input headroom and setup diagnostic scenarios\n";
}
