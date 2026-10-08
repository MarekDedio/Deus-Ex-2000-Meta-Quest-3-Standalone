#include "portable_lightmap_math.h"
#include "portable_model_geometry.h"
#include <cstring>
#include "Math/hsb.h" // Actual pinned color table/function: differential oracle.
#include "Math/coords.h" // Actual pinned rotation convention: differential oracle.

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>

namespace {
using namespace QuestVr;

void Require(bool condition,const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void Near(float actual,float expected,const char* message,float epsilon=.00001f) {
    if (!std::isfinite(actual) || !std::isfinite(expected) || std::fabs(actual-expected)>epsilon)
        throw std::runtime_error(message);
}
void Near(const LightmapVec3& actual,const LightmapVec3& expected,
          const char* message,float epsilon=.00001f) {
    Near(actual.x,expected.x,message,epsilon); Near(actual.y,expected.y,message,epsilon);
    Near(actual.z,expected.z,message,epsilon);
}
LightmapVec3 Convert(const vec3& value) { return {value.x,value.y,value.z}; }

void ColorDifferential() {
    for (unsigned brightness=0; brightness<256; ++brightness) {
        const auto grey=PortableLightmapHsbColor(0,255,static_cast<std::uint8_t>(brightness));
        Near(grey.x,hsbtorgb_v_table[brightness]*(1.0f/255.0f),
             "six-decimal pinned brightness table differs",0.0f);
        for (unsigned hue=0; hue<256; ++hue)
            for (const std::uint8_t saturation : {0u,79u,80u,81u,128u,249u,250u,255u}) {
                const auto actual=PortableLightmapHsbColor(static_cast<std::uint8_t>(hue),
                    saturation,static_cast<std::uint8_t>(brightness));
                const auto expected=hsbtorgb(static_cast<std::uint8_t>(hue),saturation,
                    static_cast<std::uint8_t>(brightness));
                Near(actual,Convert(expected),"HSB differs from pinned function",0.0f);
            }
    }
}

float ShadowOracle(const std::vector<std::uint8_t>& bits,std::size_t offset,
                   std::size_t width,std::size_t height,std::size_t x,std::size_t y) {
    const auto pitch=(width+7u)/8u;
    const auto bit=[&](std::size_t xx,std::size_t yy) {
        return static_cast<float>((bits[offset+yy*pitch+(xx>>3u)]>>(xx&7u))&1u);
    };
    if (width<=2u || height<=2u) return bit(x,y);
    // Independently expand the active pinned sliding-window's bit positions.
    // This is intentionally not the disabled #if 0 clamped Gaussian branch.
    if (x==width-1u) --x;
    std::size_t columns[3];
    if (x==0u) { columns[0]=0;columns[1]=1;columns[2]=2; }
    else if (x==1u) { columns[0]=1;columns[1]=0;columns[2]=2; }
    else if (x==2u) { columns[0]=0;columns[1]=2;columns[2]=3; }
    else { columns[0]=x-1u;columns[1]=x;columns[2]=x+1u; }
    const std::size_t rows[3]={y ? y-1u:y,y,y+1u<height ? y+1u:y};
    constexpr float weights[3][3]={{.125f,.25f,.125f},{.25f,.5f,.25f},{.125f,.25f,.125f}};
    float result{};
    for (std::size_t yy=0; yy<3; ++yy)
        for (std::size_t xx=0; xx<3; ++xx)
            result+=bit(columns[xx],rows[yy])*weights[yy][xx];
    return result;
}

void ShadowMaskCases() {
    std::mt19937 random(0x53484457u);
    std::string error;
    for (std::uint32_t height=1; height<=9; ++height)
        for (std::uint32_t width=1; width<=25; ++width) {
            const std::size_t pitch=(width+7u)/8u;
            std::vector<std::uint8_t> bits(5u+pitch*height+7u);
            for (auto& byte:bits) byte=static_cast<std::uint8_t>(random());
            std::vector<float> decoded;
            Require(DecodePortableShadowMask(bits,5,width,height,decoded,error),"valid mask rejected");
            Require(decoded.size()==static_cast<std::size_t>(width)*height,"mask pixel count differs");
            for (std::size_t y=0; y<height; ++y)
                for (std::size_t x=0; x<width; ++x)
                    Near(decoded[y*width+x],ShadowOracle(bits,5,width,height,x,y),
                         "bit order, padded pitch or pinned blur edge differs",0.0f);
        }
    std::vector<float> decoded;
    Require(DecodePortableShadowMask(std::vector<std::uint8_t>(3,255),0,3,3,decoded,error),
            "white mask rejected");
    for (const float value:decoded) Near(value,2.0f,"blur must retain pinned unnormalized sum",0.0f);
    Require(DecodePortableShadowMask({5u,5u},0,3,2,decoded,error),"small-height mask rejected");
    Require(decoded==std::vector<float>({1,0,1,1,0,1}),"small-height must not blur");
    const auto previous=decoded;
    Require(!DecodePortableShadowMask({1u},0,9,1,decoded,error),"truncated padded row accepted");
    Require(decoded==previous,"failed mask changed previous output");
    Require(!DecodePortableShadowMask({1u},std::numeric_limits<std::size_t>::max(),1,1,decoded,error),
            "overflow offset accepted");
    Require(!DecodePortableShadowMask({},0,0,1,decoded,error),"zero dimension accepted");
    PortableLightmapLimits limits; limits.maximumPixels=8;
    Require(!DecodePortableShadowMask({255u,255u,255u},0,3,3,decoded,error,limits),
            "mask allocation pixel budget ignored");
}

// Pinned CalcWorldLocations scanline formula copied as an explicit differential
// fixture, not a second production implementation. Its singular divisions are
// intentionally exercised separately by the analytic implementation.
std::vector<vec3> PinnedWorldLocations(const PortableLightmapBasis& basis) {
    const vec3 origin{basis.origin.x,basis.origin.y,basis.origin.z};
    const vec3 u{basis.textureU.x,basis.textureU.y,basis.textureU.z};
    const vec3 v{basis.textureV.x,basis.textureV.y,basis.textureV.z};
    const float uPan=dot(u,origin)+basis.panX-.5f*basis.uScale;
    const float vPan=dot(v,origin)+basis.panY-.5f*basis.vScale;
    const vec3 p[3]={origin,origin+u,origin+v};
    vec2 uv[3];
    for (int j=0;j<3;++j)
        uv[j]={(dot(u,p[j])-uPan)/basis.uScale,(dot(v,p[j])-vPan)/basis.vScale};
    const float leftDx=uv[2].x-uv[0].x,leftDy=uv[2].y-uv[0].y;
    const float rightDx=uv[2].x-uv[1].x,rightDy=uv[2].y-uv[1].y;
    std::vector<vec3> output(static_cast<std::size_t>(basis.width)*basis.height);
    for (std::size_t y=0;y<basis.height;++y) {
        float x0=uv[0].x+leftDx/leftDy*(static_cast<float>(y)+.5f-uv[0].y)+.5f;
        float x1=uv[1].x+rightDx/rightDy*(static_cast<float>(y)+.5f-uv[1].y)+.5f;
        vec3 p0=mix(p[0],p[2],(static_cast<float>(y)+.5f-uv[0].y)/leftDy);
        vec3 p1=mix(p[1],p[2],(static_cast<float>(y)+.5f-uv[1].y)/rightDy);
        if (x1<x0) { std::swap(x0,x1);std::swap(p0,p1); }
        for (std::size_t x=0;x<basis.width;++x)
            output[y*basis.width+x]=mix(p0,p1,(static_cast<float>(x)+.5f-x0)/(x1-x0));
    }
    return output;
}

PortableModelGeometry ModelFixture(std::uint32_t width=3,std::uint32_t height=3) {
    PortableModelGeometry model;
    model.points={{0,0,0}}; model.vectors={{1,0,0},{0,1,0},{0,0,1}};
    PortableModelSurface surface;
    surface.basePoint=0;surface.textureU=0;surface.textureV=1;surface.normalVector=2;surface.lightMap=0;
    model.surfaces={surface};
    PortableModelLightMapIndex lm;
    lm.uClamp=static_cast<std::int32_t>(width);lm.vClamp=static_cast<std::int32_t>(height);
    lm.uScale=2;lm.vScale=2;lm.lightActors=-1;
    model.lightMaps={lm}; return model;
}

void WorldLocationsAndUv() {
    std::string error;
    PortableLightmapBasis basis;
    basis.origin={20,40,-5};basis.textureU={2,.5f,0};basis.textureV={.3f,1.5f,0};
    basis.normal={0,0,1};basis.panX=5;basis.panY=-3;
    basis.uScale=4;basis.vScale=2;basis.width=7;basis.height=5;
    std::vector<LightmapVec3> points;
    Require(BuildPortableLightmapWorldLocations(basis,points,error),"skew texture basis rejected");
    const auto pinned=PinnedWorldLocations(basis);
    for (std::size_t i=0;i<points.size();++i)
        Near(points[i],Convert(pinned[i]),"analytic locations differ from pinned scanlines",.004f);
    auto model=ModelFixture();
    Require(GetPortableSurfaceLightmapBasis(model,0,basis,error),"model basis rejected");
    Require(BuildPortableLightmapWorldLocations(basis,points,error),"orthogonal analytic basis failed");
    Near(points[0],{-1,0,0},"pinned asymmetric half-texel convention changed");
    for (std::uint32_t y=0;y<basis.height;++y)
        for (std::uint32_t x=0;x<basis.width;++x) {
            LightmapUv uv;
            Require(SurfaceLightmapUv(model,0,points[y*basis.width+x],uv,error),"UV rejected valid world point");
            Near(uv.u,static_cast<float>(x)/basis.width,"original renderer U half-texel differs");
            Near(uv.v,(static_cast<float>(y)+.5f)/basis.height,"V texel-centre roundtrip differs");
            const auto& point=points[y*basis.width+x];
            const float authoredU=(point.x-basis.origin.x)*basis.textureU.x+
                (point.y-basis.origin.y)*basis.textureU.y+(point.z-basis.origin.z)*basis.textureU.z;
            const float authoredV=(point.x-basis.origin.x)*basis.textureV.x+
                (point.y-basis.origin.y)*basis.textureV.y+(point.z-basis.origin.z)*basis.textureV.z;
            Near(uv.u,(authoredU-basis.panX+.5f*basis.uScale)/(basis.uScale*basis.width),
                 "surface U differs from pinned GLRenderDevice equation");
            Near(uv.v,(authoredV-basis.panY+.5f*basis.vScale)/(basis.vScale*basis.height),
                 "surface V differs from pinned GLRenderDevice equation");
        }
    const auto previous=points;
    basis.textureV=basis.textureU;
    Require(!BuildPortableLightmapWorldLocations(basis,points,error),"singular texture basis accepted");
    Require(points.size()==previous.size(),"failed location generation changed output");
    for (std::size_t i=0;i<points.size();++i) Near(points[i],previous[i],"failure replaced location data",0.0f);
    basis.textureV={0,1,0};basis.uScale=0;
    Require(!BuildPortableLightmapWorldLocations(basis,points,error),"zero scale accepted");
    model.surfaces[0].normalVector=99;
    Require(!GetPortableSurfaceLightmapBasis(model,0,basis,error),"bad surface reference accepted");
}

float PinnedFalloff(float distanceSquared) {
    const float v=std::sqrt(distanceSquared+.0001f),v2=v*v,v3=v2*v;
    return std::min((1.0f+2.0f*v3-3.0f*v2)/v,1.0f);
}

void StaticEffects() {
    PortableStaticLight light;light.available=true;light.position={0,0,12.5f};
    light.brightness=255;light.radius=0;light.cone=128;
    float result{};bool coincident{};
    Require(EvaluatePortableStaticLight(light,{0,0,0},{0,0,1},2,result,coincident),"default effect failed");
    Near(result,2*PinnedFalloff(.25f),"default radius/incidence/falloff differs");
    Require(EvaluatePortableStaticLight(light,{0,0,0},{0,0,-1},1,result,coincident),"back-facing effect failed");
    Near(result,PinnedFalloff(.25f),"pinned absolute incidence changed");
    Require(EvaluatePortableStaticLight(light,{0,0,-12.5f},{0,0,1},1,result,coincident),"radius edge rejected");
    Near(result,0,"radius edge illuminated");
    Require(EvaluatePortableStaticLight(light,light.position,{0,0,1},1,result,coincident),"singular sample rejected");
    Require(coincident && result==0,"singular normalize leaked NaN without diagnostics");
    light.effect=13;
    Require(EvaluatePortableStaticLight(light,{0,0,0},{0,0,0},1,result,coincident),"non-incidence failed");
    Near(result,.5f,"non-incidence linear radius differs");
    light.effect=17;
    Require(EvaluatePortableStaticLight(light,{0,0,-1000},{0,0,0},1,result,coincident),"cylinder failed");
    Near(result,1,"cylinder must ignore Z");
    light.effect=14;light.position={0,0,22.5f};
    Require(EvaluatePortableStaticLight(light,{0,0,0},{0,0,0},1,result,coincident),"shell failed");
    Near(result,1,"shell peak differs");
    light.effect=15;
    Require(EvaluatePortableStaticLight(light,{0,0,0},{0,0,0},1,result,coincident),"blacklight failed");
    Near(result,0,"OmniBumpMap must be black");
    light.effect=5;
    Require(!EvaluatePortableStaticLight(light,{0,0,0},{0,0,1},1,result,coincident),
            "dynamic wave silently frozen into static bake");
    light.effect=0;light.position.x=std::numeric_limits<float>::infinity();
    Require(!EvaluatePortableStaticLight(light,{0,0,0},{0,0,1},1,result,coincident),"non-finite light accepted");
}

void NonnegativeRadiusEdge() {
    constexpr std::uint32_t sampleBits = 0x41c7f29du;
    float sample{}; std::memcpy(&sample, &sampleBits, sizeof(sample));
    const float d = sample*sample*((1.0f/25.0f)*(1.0f/25.0f));
    const float v = std::sqrt(d+.0001f), v2 = v*v, v3 = v2*v;
    const float contracted = std::fma(-3.0f, v2, std::fma(2.0f, v3, 1.0f))/v;
    Require(contracted < 0.0f, "FMA cancellation regression fixture no longer exercises a negative expanded polynomial");
    std::size_t samples{};
    for (std::uint32_t bits = sampleBits-32768u; bits < sampleBits+32768u; ++bits) {
        float z{}; std::memcpy(&z, &bits, sizeof(z));
        for (const std::uint8_t effect : {0u,8u,12u}) {
            PortableStaticLight light; light.position={0,0,z}; light.radius=0;
            light.effect=effect; light.cone=128; light.pitch=-16384;
            float actual{}; bool coincident{};
            Require(EvaluatePortableStaticLight(light,{0,0,0},{0,0,1},1.0f,actual,coincident),
                    "Radius edge illumination fixture rejected");
            Require(std::isfinite(actual) && actual>=0.0f && actual<=1.0f,
                    "Radius edge illumination became negative, nonfinite or exceeded one");
            if (effect==0u) {
                const float distance=z*z*((1.0f/25.0f)*(1.0f/25.0f));
                const double root=std::sqrt(static_cast<double>(distance+.0001f));
                const double edge=1.0-root;
                const float expected=distance>=1.0f ? 0.0f :
                    static_cast<float>(std::min(edge*edge*(1.0+2.0*root)/root,1.0));
                Near(actual,expected,"Factored falloff differs from double analytic oracle",.00000001f);
            }
            ++samples;
        }
    }
    std::cout << "nonnegative radius-edge controls=" << samples << " (expanded FMA fixture negative)\n";
}

void SpotlightRotationDifferential() {
    std::mt19937 random(0x53504f54u);
    for (std::size_t i=0;i<4096;++i) {
        PortableStaticLight light;light.effect=8;light.radius=2;light.cone=128;
        light.yaw=static_cast<std::int32_t>(random());light.pitch=static_cast<std::int32_t>(random());
        const auto rotation=Coords::Rotation(Rotator(light.pitch,light.yaw,0));
        const vec3 direction=-rotation.XAxis;
        const LightmapVec3 location{-direction.x*10,-direction.y*10,-direction.z*10};
        float actual{};bool coincident{};
        Require(EvaluatePortableStaticLight(light,location,Convert(direction),1,actual,coincident),
                "spot effect rejected rotation fixture");
        const float distanceSquared=100.0f*((1.0f/75.0f)*(1.0f/75.0f));
        Near(actual,PinnedFalloff(distanceSquared),"spot direction differs from pinned Coords axes",.00002f);
        light.cone=0;
        Require(EvaluatePortableStaticLight(light,location,Convert(direction),1,actual,coincident),
                "empty spot cone rejected");
        Near(actual,0,"zero cone must not illuminate");
    }
    PortableStaticLight light;light.effect=8;light.radius=1;light.cone=128;
    float actual{};bool coincident{};
    const float cosine=.75f,sine=std::sqrt(1.0f-cosine*cosine);
    const LightmapVec3 direction{-cosine,sine,0};
    const LightmapVec3 point{cosine*10,-sine*10,0};
    Require(EvaluatePortableStaticLight(light,point,direction,1,actual,coincident),"off-axis cone failed");
    const float outer=1.0f-128.0f*(1.0f/255.0f);
    const float spot=1.0f-std::min((1.0f-cosine)/(1.0f-outer),1.0f);
    Near(actual,PinnedFalloff(.04f)*spot*spot,"spot attenuation must be squared");
}

void BakeOrdinalAndAmbient() {
    auto model=ModelFixture();
    model.lightMaps[0].lightActors=0;model.lights={10,11,0};
    model.lightBits={255,255,255,0,0,0};
    PortableStaticLight disabled;disabled.available=true;disabled.objectReference=10;disabled.type=0;
    PortableStaticLight enabled;enabled.available=true;enabled.objectReference=11;enabled.brightness=255;
    enabled.position={0,0,10};enabled.radius=4;
    PortableSurfaceLightmap output;std::string error;
    const PortableLightmapZoneAmbient ambient{42,81,64};
    Require(BakeStaticSurfaceLightmap(model,0,ambient,{disabled,enabled},output,error),"valid bake rejected");
    Require(output.stats.disabledLights==1 && output.stats.addedLights==1,"disabled list entry compressed");
    const auto color=PortableLightmapHsbColor(42,81,64);
    for (const auto& pixel:output.pixels) Near(pixel,color,"disabled light shifted mask ordinal",0.0f);
    model.lightBits={0,0,0,255,255,255};
    Require(BakeStaticSurfaceLightmap(model,0,{}, {disabled,enabled},output,error),"lit second ordinal rejected");
    Require(output.pixels[0].x>0,"second authored mask not used");
    model.lights={10,11,12,0};model.lightBits={0,0,0,255,255,255,255,255,255};
    auto third=enabled;third.objectReference=12;
    Require(BakeStaticSurfaceLightmap(model,0,{}, {disabled,enabled,third},output,error),"multi-light sum rejected");
    Require(output.pixels[0].x>1.0f,"pinned total light sum was incorrectly clamped");
    const auto previous=output.pixels;
    auto unavailable=third;unavailable.available=false;
    Require(!BakeStaticSurfaceLightmap(model,0,{}, {disabled,enabled,unavailable},output,error),
            "missing authored actor silently accepted");
    Require(output.pixels.size()==previous.size(),"failed bake changed output dimensions");
    for (std::size_t i=0;i<previous.size();++i) Near(output.pixels[i],previous[i],"failed bake replaced previous RGB",0.0f);
    third.type=2;third.effect=5;
    Require(BakeStaticSurfaceLightmap(model,0,{}, {disabled,enabled,third},output,error),"diagnosed dynamic bake rejected");
    Require(output.stats.unsupportedTypes==1 && output.stats.unsupportedEffects==1 &&
            output.stats.unsupportedTypeCounts[2]==1 && output.stats.unsupportedEffectCounts[5]==1,
            "dynamic type/effect silently interpreted as steady/default");
    enabled.objectReference=99;
    Require(!BakeStaticSurfaceLightmap(model,0,{}, {disabled,enabled,third},output,error),
            "actor/mask reference mismatch accepted");
    enabled.objectReference=11;
    PortableLightmapLimits limits;limits.maximumSampleLightOperations=26;
    Require(!BakeStaticSurfaceLightmap(model,0,{}, {disabled,enabled,third},output,error,limits),
            "sample/light CPU operation budget ignored");
    model.lightBits.resize(8);
    Require(!BakeStaticSurfaceLightmap(model,0,{}, {disabled,enabled,third},output,error),
            "truncated unsupported light shadow span accepted");
}
} // namespace

int main() {
    try {
        ColorDifferential();ShadowMaskCases();WorldLocationsAndUv();StaticEffects();
        NonnegativeRadiusEdge();SpotlightRotationDifferential();BakeOrdinalAndAmbient();
        std::cout << "portable_lightmap_math: 7 groups passed (pinned HSB/rotation, padded masks, "
                     "blur edges, analytic world/UV, static effects, ordered bake/rejection)\n";
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << "portable_lightmap_math: " << failure.what() << '\n';return 1;
    }
}
