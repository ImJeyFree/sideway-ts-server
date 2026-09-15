#include "TunerClient.h"
#include <iostream>
int main(int argc,char* argv[]) {
    if(argc!=2)return 2;
    try{TsBroadcaster hub;TunerClient engine(hub,"unused.json");std::cerr<<"잘못된 DLL이 허용됐습니다\n";return 1;}
    catch(const std::exception& e){
        const std::string expected=std::string(argv[1])=="version"?"API 버전 불일치":"SidewayTunerCore.dll을 실행 파일 옆에 설치";
        if(std::string(e.what()).find(expected)==std::string::npos){std::cerr<<e.what();return 1;}
        std::cout<<"PASS: DLL 로딩 오류 안내\n";return 0;
    }
}
