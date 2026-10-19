// Independent test-only protocol of Engine10323770..103239A0 and imported
// Core.SafeNormal10144BE0..10144C5E. No production numeric helper is called.
namespace TwoWallOracle {
using Vec=PortableModelVec3;
struct Census {std::size_t corner{},face{},safeZero{},safeNormalize{},flips{},zeros{};};
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
inline constexpr bool Available=true;
using Real=long double;
struct Precision {
    unsigned short previous{};
    Precision() {
        static_assert(std::numeric_limits<Real>::digits==64,"Two-wall oracle needs hardware x87 PC64");
        asm volatile("fnstcw %0":"=m"(previous));
        const auto mode=static_cast<unsigned short>((previous&~0x0f00u)|0x033fu);
        asm volatile("fldcw %0"::"m"(mode));
    }
    ~Precision(){asm volatile("fldcw %0"::"m"(previous));}
    Precision(const Precision&)=delete;
    Precision& operator=(const Precision&)=delete;
};
Real Read(float value) {
    if(!std::isfinite(value))throw std::runtime_error("Two-wall x87 active input nonfinite");
    return static_cast<Real>(value);
}
Real Add(Real a,Real b){volatile Real x=a,y=b,result=x+y;return result;}
Real Sub(Real a,Real b){volatile Real x=a,y=b,result=x-y;return result;}
Real Mul(Real a,Real b){volatile Real x=a,y=b,result=x*y;return result;}
Real Div(Real a,Real b){volatile Real x=a,y=b,result=x/y;return result;}
float Store(Real value) {
    volatile float result=static_cast<float>(value);
    if(!std::isfinite(result))throw std::runtime_error("Two-wall x87 active Float store overflow");
    return result;
}
double Spill(Real value) {
    volatile double result=static_cast<double>(value);
    if(!std::isfinite(result))throw std::runtime_error("Two-wall x87 active Double store overflow");
    return result;
}
Real Root(double value) {
    Real result;asm volatile("fldl %1; fsqrt; fstpt %0":"=m"(result):"m"(value):"st");return result;
}
Vec Normalize(Vec cross,Census& census) {
    const auto square=Add(Add(Mul(Read(cross.x),Read(cross.x)),Mul(Read(cross.y),Read(cross.y))),
        Mul(Read(cross.z),Read(cross.z)));
    const auto saved=Spill(square);
    constexpr double threshold=std::bit_cast<double>(std::uint64_t{0x3e45798ee2308c3aull});
    if(square<static_cast<Real>(threshold)){++census.safeZero;return {};}
    ++census.safeNormalize;const auto inverse=Div(1.0L,Root(saved));
    return {Store(Mul(inverse,Read(cross.x))),Store(Mul(inverse,Read(cross.y))),Store(Mul(inverse,Read(cross.z)))};
}
// The deliberately wrong variants are observability controls only. Acceptance
// always uses the default exact protocol. They establish that a cohort would
// detect an accidental Float spill at the retained factor or comparison gates.
enum class Variant { Exact,StoredNormalGate,StoredDesiredGate,StoredCornerFactor };
void Adjust(const Vec& desired,Vec& delta,const Vec& normal,const Vec& old,float time,
    Census& census,Variant variant=Variant::Exact) {
    auto normalDot=Add(Add(Mul(Read(normal.y),Read(old.y)),Mul(Read(old.z),Read(normal.z))),
        Mul(Read(normal.x),Read(old.x)));
    if(variant==Variant::StoredNormalGate)normalDot=Store(normalDot);
    if(normalDot<=0.0L) {
        ++census.corner;
        // The source builds Z, Y, X on its x87 stack, then stores XYZ.
        const auto z=Sub(Mul(Read(normal.x),Read(old.y)),Mul(Read(normal.y),Read(old.x)));
        const auto y=Sub(Mul(Read(old.x),Read(normal.z)),Mul(Read(normal.x),Read(old.z)));
        const auto x=Sub(Mul(Read(normal.y),Read(old.z)),Mul(Read(normal.z),Read(old.y)));
        const Vec cross{Store(x),Store(y),Store(z)};const auto axis=Normalize(cross,census);
        const auto along=Add(Add(Mul(Read(axis.z),Read(delta.z)),Mul(Read(axis.y),Read(delta.y))),
            Mul(Read(axis.x),Read(delta.x)));
        auto factor=Mul(along,Sub(1.0L,Read(time)));
        if(variant==Variant::StoredCornerFactor)factor=Store(factor);
        const auto dx=Store(Mul(Read(axis.x),factor));
        const auto dy=Store(Mul(Read(axis.y),factor));
        const auto dz=Store(Mul(Read(axis.z),factor));delta={dx,dy,dz};
        // Desired is an actual reference and may be the just-written Delta.
        auto gate=Add(Add(Mul(Read(desired.z),Read(delta.z)),Mul(Read(desired.y),Read(delta.y))),
            Mul(Read(desired.x),Read(delta.x)));
        if(variant==Variant::StoredDesiredGate)gate=Store(gate);
        if(gate<0.0L) {
            ++census.flips;
            const auto nx=Store(Mul(Read(delta.x),-1.0L)),ny=Store(Mul(Read(delta.y),-1.0L)),
                nz=Store(Mul(Read(delta.z),-1.0L));delta={nx,ny,nz};
        }
    }else {
        ++census.face;
        const auto dot=Add(Add(Mul(Read(normal.y),Read(delta.y)),Mul(Read(delta.x),Read(normal.x))),
            Mul(Read(delta.z),Read(normal.z)));
        const auto projectedZ=Store(Mul(dot,Read(normal.z))),projectedY=Store(Mul(dot,Read(normal.y)));
        const auto projectedX=Mul(dot,Read(normal.x));
        const auto residualZ=Store(Sub(Read(delta.z),Read(projectedZ))),residualY=Store(Sub(Read(delta.y),Read(projectedY))),
            residualX=Store(Sub(Read(delta.x),projectedX));
        const auto factor=Store(Sub(1.0L,Read(time)));
        // The source retains scaled Z/Y/X before storing all three results.
        const auto dz=Mul(Read(residualZ),Read(factor)),dy=Mul(Read(residualY),Read(factor)),
            dx=Mul(Read(residualX),Read(factor));
        const auto savedX=Store(dx),savedY=Store(dy),savedZ=Store(dz);delta={savedX,savedY,savedZ};
        auto gate=Add(Add(Mul(Read(desired.z),Read(delta.z)),Mul(Read(desired.y),Read(delta.y))),
            Mul(Read(desired.x),Read(delta.x)));
        if(variant==Variant::StoredDesiredGate)gate=Store(gate);
        if(gate<=0.0L){++census.zeros;delta={};}
    }
}
#else
inline constexpr bool Available=false;
#endif
} // namespace TwoWallOracle
