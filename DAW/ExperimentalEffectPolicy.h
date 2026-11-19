#pragma once
#include <cmath>
namespace idw::daw {
inline bool experimentalEffectBlocksDocumentAction(bool loading,bool loaded){return loading || loaded;}
inline bool experimentalEffectPlaybackReady(bool loading,bool loaded,bool looping,double rate,int block,double loadedRate,int loadedBlock){
    return !loading && (!loaded || (!looping && std::isfinite(rate) && rate>=8000 &&
        rate==loadedRate && block>0 && block==loadedBlock));
}
}
