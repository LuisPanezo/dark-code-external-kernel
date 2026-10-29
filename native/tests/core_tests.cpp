#include "core_tests.h"
#include "adapters/normalize.h"
#include "projection/projection.h"
#include "renderer/scene_geometry.h"
#include "runtime/frame_runtime.h"
#include "sources/mock_source.h"
#include "sources/replay_source.h"
#include <limits>
#include <stdexcept>
#include <thread>
namespace dc {
static void Need(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);}
static bool Near(float a,float b) {return std::abs(a-b)<1e-4f;}
int RunCoreTests() {
    int passed=0;
    auto frame=SampleScene(0,Coordinates::CanonicalMeters);
    Need(Validate(frame),"valid mock rejected");++passed;
    Need(Length(ToCanonical({100,200,300},Coordinates::UnrealWorldCentimeters)-Vec3{2,3,-1})<1e-5f&&
        Length(ToCanonical({2,3,1},Coordinates::UnityWorldMeters)-Vec3{2,3,-1})<1e-5f,"known coordinate conversion failed");++passed;
    for(Coordinates c:{Coordinates::UnityWorldMeters,Coordinates::UnrealWorldCentimeters}) {
        auto normalized=SampleScene(0,c);
        Need(Validate(normalized),"adapter invalid output");
        for(size_t i=0;i<frame.entities.size();++i) {
            Need(Length(frame.entities[i].position-normalized.entities[i].position)<1e-4f,"position mismatch");
            Need(Length(frame.entities[i].half_extents-normalized.entities[i].half_extents)<1e-4f,"extents mismatch");
        }
        Need(Length(frame.camera.position-normalized.camera.position)<1e-4f,"camera mismatch");++passed;
    }
    auto bad=frame;bad.entities[0].position.x=std::numeric_limits<float>::quiet_NaN();
    Need(!Validate(bad),"NaN accepted");++passed;
    bad=frame;bad.entities[1].id=bad.entities[0].id;Need(!Validate(bad),"duplicate IDs accepted");++passed;
    bad=frame;bad.camera.target=bad.camera.position;Need(!Validate(bad),"degenerate camera accepted");++passed;
    bad=frame;bad.entities.resize(kMaxEntities+1);Need(!Validate(bad),"oversize accepted");++passed;
    TelemetryFrame input;input.entities=frame.entities;input.entities[0].half_extents.x=-1;
    FrameSnapshot unchanged=frame;
    Need(Normalize(input,Coordinates::CanonicalMeters,unchanged)==Status::InvalidFrame,"negative size accepted");
    Need(unchanged.entities.size()==frame.entities.size(),"invalid adapter modified output");++passed;
    Need(IsFresh(frame,frame.timestamp_ns+1000)&&!IsFresh(frame,frame.timestamp_ns+300000000)&&
        !IsFresh(frame,frame.timestamp_ns-1),"freshness boundary failed");++passed;
    CameraFrame camera;camera.position={0,0,0};camera.target={0,0,-1};
    auto matrix=ViewProjection(camera,2);Vec3 p,q;
    Need(ProjectPoint(matrix,{0,0,-2},p)&&Near(p.x,0)&&Near(p.y,0),"center projection failed");++passed;
    Need(!ProjectPoint(matrix,{0,0,2},p),"behind camera rendered");++passed;
    Need(ProjectSegment(matrix,{-1,0,-2},{1,0,.1f},p,q)&&Finite(p)&&Finite(q)&&std::abs(q.x)<=1.001f,"near clipping failed");++passed;
    Vec3 a,b;Need(ProjectPoint(ViewProjection(camera,1),{.5f,0,-2},a)&&ProjectPoint(matrix,{.5f,0,-2},b)&&Near(a.x,b.x*2),"aspect correction failed");++passed;
    Need(BuildScene(frame,1,{true,true}).size()>0&&BuildScene(frame,1,{false,false}).empty()&&BuildScene(bad,1,{}).empty(),"geometry validation failed");++passed;
    ReplaySource empty({},33333333);Need(empty.Start()==Status::InvalidFrame,"empty replay accepted");++passed;
    ReplaySource replay({frame},1,false);Need(replay.Start()==Status::Ok,"replay start failed");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));FrameSnapshot output;
    Need(replay.ReadLatest(output)==Status::EndOfStream,"replay did not end");replay.Stop();
    Need(replay.ReadLatest(output)==Status::Stopped,"replay not stopped");++passed;
    FrameRuntime runtime;runtime.Start();runtime.Start();
    LatestFrame current;
    for(int i=0;i<40;++i) {current=runtime.Latest();if(current.frame)break;std::this_thread::sleep_for(std::chrono::milliseconds(10));}
    Need(current.frame!=nullptr,"runtime failed to publish");auto retained=current.frame;
    runtime.SetMode(2);runtime.SetPaused(true);
    for(int i=0;i<40;++i) {current=runtime.Latest();if(current.frame)break;std::this_thread::sleep_for(std::chrono::milliseconds(10));}
    Need(current.frame&&current.status==Status::Paused,"mode change while paused failed");
    Need(Validate(*retained),"published snapshot mutated");
    runtime.Stop();runtime.Stop();Need(runtime.Latest().status==Status::Stopped,"stop failed");++passed;
    FrameRuntime broken([](int)->std::unique_ptr<IFrameSource>{throw std::runtime_error("offline");});
    broken.Start();
    for(int i=0;i<40&&broken.Latest().status!=Status::Disconnected;++i)std::this_thread::sleep_for(std::chrono::milliseconds(5));
    Need(broken.Latest().status==Status::Disconnected&&!broken.Latest().frame,"source failure did not fail closed");
    broken.Stop();++passed;
    return passed;
}
}
