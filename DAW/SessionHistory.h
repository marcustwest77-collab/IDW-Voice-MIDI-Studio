#pragma once
#include "SessionModel.h"
namespace idw::daw {
inline bool restoreSessionHistory(Session& session,std::vector<Session>& from,std::vector<Session>& to){
    if(from.empty())return false;
    if(to.size()>=64)to.erase(to.begin());
    to.push_back(session);
    session=std::move(from.back());from.pop_back();
    return true;
}
}
