#include "UnitTest.h"
#include <formatstruct.h>


enum class Color
{
    Red,
    Pink,
    Black
};
struct Nested
{
    std::vector<int>           ids;
    std::map<std::string, int> amap; // maps, vectors, arrays are auto formated.
};
struct A
{
    int         id;
    std::string name;
    Color       color; // enum is formated to name of string
    Nested      nested;
};

TEST_CASE( "formatstruct - basic" )
{
    A a = { .id = 1, .name = "John", .color = Color::Pink, .nested = Nested{ .ids = { 2, 3, 4 }, .amap = { { "A", 10 }, { "B", 20 } } } };

    std::string res =
            R"( { "id" : 1 , "name" : "John" , "color" : "Pink" , "nested" :  { "ids" :  [ 2 , 3 , 4 ]  , "amap" :  { "A" : 10 , "B" : 20 }  }  } )";
    CHECK_EQ( jz::stringify_struct( a ), res );
}

//==================================================================================
//    Test print bitfield and pointer
//==================================================================================

struct Account
{
    uint32_t hasAccount : 1; // bitfield
    uint32_t flags : 2;
    uint32_t amount;
};

struct User
{
    int                                id;
    std::string                        name;
    Account                            account;
    std::vector<std::unique_ptr<User>> friends; // pointer is also dereferenced.
};

namespace jz
{

/// User could either: 1) let this trait be empty, then framework will auto detect all members
///                    2) define GetStructMembersTuple to return members.
///                    3) define OverrideMemberAccessors to override the members that are auto detected by 1).
template<>
struct FormatStructTrait<User>
{
    static constexpr auto OverrideMemberAccessors()
    {
        return jz::make_struct_members<MEMBER_ACCESSOR( User, "id", []( User const &obj ) { return "id" + std::to_string( obj.id ); } )>();
    }
};
static_assert( jz::HasOverrideMemberAccessors<User> );

/// User could either: 1) let this trait be empty, then framework will auto detect all members
///                    2) define GetStructMembersTuple to return members.
///                    3) define OverrideMemberAccessors to override the members that are auto detected by 1).
template<>
struct FormatStructTrait<Account>
{
    static constexpr auto GetStructMembersTuple()
    {
        using U = Account;
        return jz::make_struct_members<BITFIELD_ACCESSOR( Account, hasAccount ), //
                                       BITFIELD_ACCESSOR( Account, flags ),      //
                                       &U::amount>();
    }
};
static_assert( jz::HasGetStructMembersTuple<Account> );

} // namespace jz

TEST_CASE( "formatstruct - bitfield, pointer" )
{
    CHECK_EQ( jz::getTypeName<Account>(), "Account" );
    CHECK_EQ( jz::getStructMemberName<&Account::amount>(), "amount" );

    Account account{ .hasAccount = 1, .flags = 3, .amount = 100 };
    // std::cout << "|" << jz::StructPrinter{ account } << "|" << std::endl;
    std::string res = R"( { "hasAccount" : 1 , "flags" : 3 , "amount" : 100 } )";
    CHECK_EQ( res, jz::stringify_struct( account ) );

    auto a = std::make_unique<User>( 1, "John", Account{ .hasAccount = 1, .flags = 3, .amount = 100 } );
    a->friends.push_back( std::make_unique<User>( 2, "Bob" ) );
    a->friends.push_back( std::make_unique<User>( 3, "Alice", Account{ .hasAccount = 1, .flags = 1, .amount = 300 } ) );
    // std::cout << jz::StructPrinter{ a } << std::endl;
    res = R"( { "id" : "id1" , "name" : "John" , "account" :  { "hasAccount" : 1 , "flags" : 3 , "amount" : 100 }  , "friends" :  [  { "id" : "id2" , "name" : "Bob" , "account" :  { "amount" : 0 }  , "friends" :  [  ]  }  ,  { "id" : "id3" , "name" : "Alice" , "account" :  { "hasAccount" : 1 , "flags" : 1 , "amount" : 300 }  , "friends" :  [  ]  }  ]  } )";
    CHECK_EQ( res, jz::stringify_struct( a ) );
}
