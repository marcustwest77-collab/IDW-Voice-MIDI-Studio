#include "SetupDiagnostics.h"
#include <iostream>
#include <stdexcept>
int main(){using namespace idw;
 auto check=[](SetupState got,SetupState wanted){if(got!=wanted)throw std::runtime_error("Wrong setup guidance");};
 check(diagnose(false,.1f,.08f,.01f,false,true,false,.9f,.75f,true),SetupState::stopped);
 check(diagnose(true,1,.08f,.01f,false,true,false,.9f,.75f,true),SetupState::clipping);
 check(diagnose(true,0,0,.01f,false,true,false,0,.75f,false),SetupState::silent);
 check(diagnose(true,.1f,.08f,.01f,true,true,false,.9f,.75f,true),SetupState::training);
 check(diagnose(true,.1f,.08f,.01f,false,false,false,.9f,.75f,false),SetupState::disabled);
 check(diagnose(true,.01f,.005f,.01f,false,true,false,.9f,.75f,false),SetupState::belowGate);
 check(diagnose(true,.1f,.08f,.01f,false,true,false,.4f,.75f,false),SetupState::uncertain);
 check(diagnose(true,.1f,.08f,.01f,false,true,false,.9f,.75f,true),SetupState::tracking);
 check(diagnose(true,.1f,.005f,.01f,false,false,true,0,.75f,false),SetupState::listening);
 std::cout<<"PASS: nine setup diagnostic scenarios\n";
}
