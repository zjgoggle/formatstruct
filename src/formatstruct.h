#pragma once


/// format struct auto prints aggregated structs.

#include <boost/pfr.hpp>
#include <magic_enum/magic_enum.hpp>

#include <variant>
#include <map>
#include <unordered_map>
#include <optional>
#include <string_view>
#include <type_traits>

namespace jz {
template<class T>
struct IsVariant : std::false_type {};
template<class... Args>
struct IsVariant<std::variant<Args...>> : std::true_type {};

template<class T>
concept LikeMap = requires {
    typename T::mapped_type;
    typename T::key_type;
};

template<class T>
concept LikeVec = requires(T vec, size_t n) {
    { std::begin(vec) };
    { std::end(vec) };
    { vec[n] };
} && !requires { typename T::key_type; };

template<class T>
struct IsStr : std::false_type {};
template<>
struct IsStr<std::string> : std::true_type {};
template<>
struct IsStr<std::string_view> : std::true_type {};
template<size_t N>
struct IsStr<char[N]> : std::true_type {};

template<class T, typename = void>
constexpr bool HasDeref = false;
template<class T>
constexpr bool HasDeref<T, std::void_t<decltype(*std::declval<T>())>> = true;

template<class T, typename = void>
constexpr bool HasBoolOperator = false;
template<class T>
constexpr bool HasBoolOperator<T, std::void_t<decltype(bool(std::declval<T>()))>> = true;

template<class T>
constexpr bool IsLikePointer = HasDeref<T> && HasBoolOperator<T>;
static_assert(IsLikePointer<char *>);
static_assert(IsLikePointer<std::unique_ptr<char *>>);
static_assert(IsLikePointer<std::optional<char *>>);

template<size_t N>
struct StringLit {
    char value[N];

    constexpr StringLit(const char (&str)[N]) { std::copy_n(str, N, value); }

    constexpr auto operator<=>(const StringLit &) const = default;

    constexpr std::string_view view() const { return std::string_view(value, N - 1); }
};

inline constexpr std::string_view type_name_removes_kind(std::string_view className) {
#ifdef _MSC_VER
    if (className.starts_with("enum ")) return className.substr(5);
    if (className.starts_with("union ")) return className.substr(6);
    if (className.starts_with("class ")) return className.substr(6);
    if (className.starts_with("struct ")) return className.substr(7);
#endif
    return className;
}

template<class T>
constexpr std::string_view getTypeName() {
#ifdef _MSC_VER
    // with prefix class/struct/enum/union
    std::string_view s = __FUNCSIG__, starts = "getTypeName<", ends = ">(void)";
#elif defined(__clang__) // must be before __GNUC__ which includes clang
    std::string_view s = __PRETTY_FUNCTION__, starts = "getTypeName() [with T =  ", ends = ";";
#elif defined(__GNUC__) || defined(__GNUG__)
    std::string_view s = __PRETTY_FUNCTION__, starts = "getTypeName() [with T = ", ends = ";";
#else
    static_assert(false, "Unsupported compiler!");
#endif
    auto i = s.find(starts) + starts.size();
    auto j = s.rfind((ends));
    return type_name_removes_kind(s.substr(i, j - i));
}

#if defined(_MSC_VER) && (_MSC_VER < 1940)
#pragma message("WARN: getVariableName function is not supported by low msvc")
constexpr bool support_getVariableName = false;
#else
constexpr bool support_getVariableName = true;
#endif

template<auto value>
constexpr std::string_view getVariableName() {
#ifdef _MSC_VER
    static_assert(support_getVariableName);
    // with prefix class/struct/enum/union
    std::string_view s = __FUNCSIG__, starts = "getVariableName<", ends = ">(void)";
#elif defined(__clang__) // must be before __GNUC__ which includes clang
    std::string_view s = __PRETTY_FUNCTION__, starts = "getVariableName() [value = ", ends = "]";
#elif defined(__GNUC__) || defined(__GNUG__)
    std::string_view s = __PRETTY_FUNCTION__, starts = "getVariableName() [with auto value = ", ends = ";";
#else
    static_assert(false, "Unsupported compiler!");
#endif
    auto i = s.find(starts) + starts.size();
    auto j = s.rfind((ends));
    return type_name_removes_kind(s.substr(i, j - i));
}

template<auto member>
constexpr std::pair<std::string_view, std::string_view> getStructNameAndMemberName() {
    std::string_view s = getVariableName<member>();
    auto             k = s.rfind("::");
    return {s.substr(1, k - 1), s.substr(k + 2)};
}

template<auto pObjMember>
constexpr std::string_view getStructMemberName() {
    return getStructNameAndMemberName<pObjMember>().second;
}

//#define HAS_ADDR_MEMBER(T, member) ([]<class U>() ->bool {return (requires { &U::member; });}).operator()<T>()
//#define HAS_MEMBER(T, member) ([]<class U>() ->bool {return (requires { U::member; });}).operator()<T>()
//#define HAS_BITFIELD_MEMBER(T, member) (HAS_MEMBER(T, member) && not HAS_ADDR_MEMBER(T, member))

#define BITFIELD_ACCESSOR(T, memberName)                                                                                                            \
    MemberGetter<T, decltype([](T const &obj) { return obj.memberName; }), StringLit{#memberName}, true> {}

#define MEMBER_ACCESSOR(T, memberNameStr, getMemberFunc)                                                                                            \
    MemberGetter<T, decltype(getMemberFunc), StringLit{memberNameStr}, false> {}

template<class T, class M, M T::*pMember>
struct MemberInfo {
    using ClassType                   = T;
    using MemberType                  = M;
    static constexpr bool IS_BITFIELD = false;
    static constexpr auto name        = getStructMemberName<pMember>();
    static constexpr M T::*member     = pMember;

    static constexpr std::string_view getName() { return name; }
    static constexpr M const         &getMember(T const &obj) { return obj.*member; }
};
template<auto pMember>
struct MemberInfoCreator;
template<class T, class M, M T::*member>
struct MemberInfoCreator<member> {
    using type = MemberInfo<T, M, member>;
};

template<class T, class GetMemberFuncT, auto memberName, bool isBitField>
struct MemberGetter {
    using ClassType                   = T;
    using MemberType                  = std::invoke_result_t<GetMemberFuncT, T const &>;
    static constexpr auto name        = memberName;
    static constexpr bool IS_BITFIELD = isBitField;

    static constexpr std::string_view getName() { return name.view(); }
    static constexpr MemberType       getMember(T const &obj) { return GetMemberFuncT{}(obj); }
};

template<class T>
struct IsMemberGetter : std::false_type {};
template<class T, class GetMemberFuncT, auto memberName, bool isBitField>
struct IsMemberGetter<MemberGetter<T, GetMemberFuncT, memberName, isBitField>> : std::true_type {};

template<class T>
struct IsMemberInfo : std::false_type {};
template<class T, class M, auto pMember>
struct IsMemberInfo<MemberInfo<T, M, pMember>> : std::true_type {};

template<class... T>
struct IsTupleMemberInfo : std::false_type {};
template<class... T>
struct IsTupleMemberInfo<std::tuple<T...>> {
    static constexpr bool value = std::conjunction_v<IsMemberInfo<T>...>;
};

template<auto pMemberOrGetter, class T = std::remove_cvref_t<decltype(pMemberOrGetter)>>
constexpr auto makeMemberInfo() {
    if constexpr (IsMemberGetter<T>::value) return pMemberOrGetter;
    else return typename MemberInfoCreator<pMemberOrGetter>::type{};
}

template<auto... members>
constexpr auto make_struct_members() {
    return std::make_tuple(makeMemberInfo<members>()...);
}


// Specialized conversion from struct members to strings. Usage.
// struct A {char x; int y;};
// struct FormatStructTrait<A> {
// static constexpr auto OverrideMemberAccessors() {
//     return jz::make_struct_members<MEMBER_ACCESSOR( A, "id", []( A const &obj ) { return "id" + std::to_string( obj.x ); } )>();
// }};

/// User could either: 1) let this trait be empty, then framework will auto detect all members
///                    2) define GetStructMembersTuple to return members.
///                    3) define OverrideMemberAccessors to override the members that are auto detected by 1).
template<class T>
struct FormatStructTrait;

template<class T, class = void>
static constexpr bool HasOverrideMemberAccessors = false;

template<class T>
static constexpr bool HasOverrideMemberAccessors<T, std::void_t<decltype(jz::FormatStructTrait<T>::OverrideMemberAccessors())>> = true;


struct FormatterGrammar {
    std::string kvBegin = " { ";
    std::string kvEnd   = " } ";
    std::string kvDelim = " , ";
    std::string kvSep   = " : ";

    std::string vecBegin = " [ ";
    std::string vecEnd   = " ] ";
    std::string vecDelim = " , ";

    bool quotedKey          = true;
    bool quotedVal          = true;
    bool ignoreZeroBitField = true; // don't print bitfield field if the value is 0.
};

template<class UserContext = int>
struct FormatContext {
    FormatterGrammar grammar;
    mutable int32_t  flattenMapLevels = 0; // number of first level of map to flatten. WHen a level is flatten, "{k1 : v1, k2: v2}" becomes "v1, v2"
    mutable UserContext userContext;

    template<class OSTREAM>
    struct ScopedMapPrinter {
        OSTREAM             &os;
        FormatContext const &context;
        int32_t              currLevel;

        ScopedMapPrinter()                                    = delete;
        ScopedMapPrinter(ScopedMapPrinter const &)            = delete;
        ScopedMapPrinter &operator=(ScopedMapPrinter const &) = delete;

        explicit ScopedMapPrinter(int32_t pcurrLevel, OSTREAM &pos, FormatContext const &pcontext)
            : os(pos), context(pcontext), currLevel(pcurrLevel) {
            if (!this->context.flattenMapLevels) { os << this->context.grammar.kvBegin; }
        }
        ~ScopedMapPrinter() {
            if (!context.flattenMapLevels) { os << context.grammar.kvEnd; }
        }
        bool canPrintKey() const { return currLevel >= context.flattenMapLevels; }
    };

    template<class OSTREAM>
    ScopedMapPrinter<OSTREAM> scopedMap(OSTREAM &os, int32_t level) const {
        return ScopedMapPrinter<OSTREAM>(level, os, *this);
    }

    template<class OSTREAM, class Key>
    OSTREAM &printKey(OSTREAM &os, const Key &name, int32_t currLevel = 0) const {
        if (currLevel >= flattenMapLevels) {
            if (grammar.quotedKey) {
                os << '\"' << name << '\"' << grammar.kvSep;
            } else {
                os << name << grammar.kvSep;
            }
        }
        return os;
    }
    template<class OSTREAM, class Val>
    OSTREAM &printVal(OSTREAM &os, const Val &val) const {
        if constexpr (std::is_enum_v<Val>) {
            if (grammar.quotedVal) {
                os << '\"' << magic_enum::enum_name(val) << '\"';
            } else {
                os << magic_enum::enum_name(val);
            }
        } else if constexpr (std::is_integral_v<Val> && sizeof(Val) > 1 || std::is_floating_point_v<Val>) { // as int
            os << val;
        } else { // as string
            if (grammar.quotedVal) {
                os << '\"' << val << '\"';
            } else {
                os << val;
            }
        }
        return os;
    }
};

//! users could implement this function to format struct.
//! E.g.
//! template<>
//! struct FormatStructTrait<PriceLevel> {
//! static constexpr auto GetStructMembersTuple() {
//!    using U = PriceLevel;
//!    return make_struct_members<&U::plSide
//!            , &U::plPrice
//!            , BITFIELD_ACCESSOR(nxtbook::PriceLevel, a_bitField)
//!            , &U::plTimestamp
//!            , &U::plOrderCount
//!            , &U::attachedOrderCount>();
//! }
//! };

template<class T, typename = void>
constexpr bool HasGetStructMembersTuple = false;

template<class T>
constexpr bool HasGetStructMembersTuple<T, std::void_t<decltype(jz::FormatStructTrait<T>::GetStructMembersTuple())>> = true;

//! users could implement this function to format struct.
template<class OSTREAM, class T, class ContextT, typename = void>
constexpr bool has_format_struct_impl = false;
template<class OSTREAM, class T, class ContextT>
constexpr bool has_format_struct_impl<OSTREAM,
                                      T,
                                      ContextT,
                                      std::void_t<decltype(jz::FormatStructTrait<T>::format_struct_impl(
                                              std::declval<OSTREAM &>(), std::declval<T const &>(), std::declval<ContextT>(), int32_t(0)))>> = true;

//! users could implement this function to format struct.
//!  struct PriceBookFormatter{
//!     template<class OSTREAM, class UserContext=int>
//!     OSTREAM& format_struct_impl(OSTREAM& os, FormatContext<UserContext> const & context = {}, int32_t currLevel = 0) const {
//!     }
//!  };
//!
template<class OSTREAM, class T, class ContextT, typename = void>
constexpr bool has_member_format_struct_impl = false;
template<class OSTREAM, class T, class ContextT>
constexpr bool has_member_format_struct_impl<
        OSTREAM,
        T,
        ContextT,
        std::void_t<decltype(std::declval<T const &>().format_struct_impl(std::declval<OSTREAM &>(), std::declval<ContextT>(), int32_t(0)))>> = true;

template<class T>
struct DetectObjType {
    using type                          = std::remove_cvref_t<T>;
    static constexpr bool is_atomic     = std::is_integral_v<type> || std::is_floating_point_v<type> || std::is_enum_v<type> || IsStr<type>::value;
    static constexpr bool is_collection = LikeVec<type> || LikeMap<type>;
    static constexpr bool is_struct     = std::is_class_v<T> && std::is_aggregate_v<T>;
    static constexpr bool is_variant    = IsVariant<T>::value;
    static constexpr bool is_pointer    = IsLikePointer<T>;
};

//! json format aggregate struct, map, vector, etc.
//! bPrintBraces only controls current level.
template<class OSTREAM, class T, class UserContext = int>
OSTREAM &format_struct(OSTREAM &os, T const &obj, FormatContext<UserContext> const &context = FormatContext{}, int32_t currLevel = 0) {
    if constexpr (has_format_struct_impl<OSTREAM, T, FormatContext<UserContext>>) {
        return jz::FormatStructTrait<T>::format_struct_impl(os, obj, context, currLevel);
    } else if constexpr (has_member_format_struct_impl<OSTREAM, T, FormatContext<UserContext>>) {
        return obj.format_struct_impl(os, context, currLevel);
    } else if constexpr (std::is_same_v<uint8_t, T>) {
        context.printVal(os, uint32_t(obj));
    } else if constexpr (std::is_enum_v<T>) {
        context.printVal(os, magic_enum::enum_name(obj));
    } else if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T> || IsStr<T>::value) {
        context.printVal(os, obj);
    } else if constexpr (IsVariant<T>::value) {
        std::visit([&](auto const &val) mutable { format_struct(os, val, context, currLevel); }, obj);
    } else if constexpr (IsLikePointer<T>) {
        using PointeeType = decltype(*std::declval<T>());
        if (obj) {
            return format_struct(os, *obj, context, currLevel);
        } else {
            if constexpr (DetectObjType<T>::is_struct || LikeMap<T>) { // empty map
                os << context.grammar.kvBegin << context.grammar.kvEnd;
            } else if constexpr (LikeVec<T>) { // empty vec
                os << context.grammar.vecBegin << context.grammar.vecEnd;
            } else { // others as empty string
                os << "\"\"";
            }
        }
    } else if constexpr (LikeVec<T>) {
        os << context.grammar.vecBegin;
        int32_t iFields = 0;
        for (auto &e : obj) {
            if (iFields++) os << context.grammar.vecDelim;
            format_struct(os, e, context, currLevel + 1);
        }
        os << context.grammar.vecEnd;
    } else if constexpr (LikeMap<T>) {
        auto    scopedMap = context.scopedMap(os, currLevel);
        int32_t iFields   = 0;
        for (auto &[name, value] : obj) {
            if (iFields++) os << context.grammar.kvDelim;
            context.printKey(os, name, currLevel);
            format_struct(os, value, context, currLevel + 1);
        };
    } else if constexpr (HasGetStructMembersTuple<T>) {
        auto    scopedMap    = context.scopedMap(os, currLevel);
        int32_t iFields      = 0;
        auto    formatMember = [&](auto &memberInfo) {
            if constexpr (memberInfo.IS_BITFIELD) {
                if (context.grammar.ignoreZeroBitField) {
                    if (auto v = memberInfo.getMember(obj)) {
                        if (iFields++) os << context.grammar.kvDelim;
                        context.printKey(os, memberInfo.getName(), currLevel);
                        os << v;
                    }
                } else {
                    if (iFields++) os << context.grammar.kvDelim;
                    context.printKey(os, memberInfo.getName(), currLevel);
                    os << memberInfo.getMember(obj);
                }
            } else {
                if (iFields++) os << context.grammar.kvDelim;
                context.printKey(os, memberInfo.getName(), currLevel);
                format_struct(os, memberInfo.getMember(obj), context, currLevel + 1);
            }
        };
        auto members = jz::FormatStructTrait<T>::GetStructMembersTuple();
        std::apply([&](auto &...elem) { (formatMember(elem), ...); }, members);
        return os;
    } else if constexpr (std::is_class_v<T> && std::is_aggregate_v<T>) {
        auto scopedMap = context.scopedMap(os, currLevel);
        boost::pfr::for_each_field_with_name(obj, [&, iFields = 0]<class U>(std::string_view name, const U &value) mutable {
            if (iFields++) os << context.grammar.kvDelim;
            context.printKey(os, name, currLevel);

            if constexpr (HasOverrideMemberAccessors<T>) {
                constexpr auto overrideMembers = jz::FormatStructTrait<T>::OverrideMemberAccessors();
                if (std::apply(
                            [&](auto const &memberGetter) {
                                if (memberGetter.name.view() == name) {
                                    format_struct(os, memberGetter.getMember(obj), context, currLevel + 1);
                                    return true; // found it
                                }
                                return false;
                            },
                            overrideMembers))
                    return;
            }

            format_struct(os, value, context, currLevel + 1);
        });
    } else {
        static_assert(sizeof(T) == -1, "unsupported T");
    }
    return os;
}

template<class T, class UserContext = int>
struct StructPrinter {
    T const                          &obj;
    FormatContext<UserContext> const &ctx;
    int32_t                           currLevel;
    StructPrinter(T const &pobj, FormatContext<UserContext> const &pcontext = FormatContext{}, int32_t pcurrLevel = 0)
        : obj(pobj), ctx(pcontext), currLevel(pcurrLevel) {}
};

template<class T, class UserContext = int>
std::ostream &operator<<(std::ostream &os, StructPrinter<T, UserContext> const &printer) {
    return format_struct(os, printer.obj, printer.ctx, printer.currLevel);
}

template<class T, class UserContext = int>
std::string stringify_struct(T const &obj, FormatContext<UserContext> const &context = FormatContext{}) {
    std::stringstream ss;
    format_struct(ss, obj, context);
    return ss.str();
}

} // namespace jz