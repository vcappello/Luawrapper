// Luawrapper.cpp : Questo file contiene la funzione 'main', in cui inizia e termina l'esecuzione del programma.
//

#include <iostream>

#include "Luawrapper.h"

int CSum(int n1, int n2)
{
	return n1 + n2;
}

std::string CConcat(std::string s1, std::string s2)
{
	return s1 + s2;
}

void test_function()
{
	std::cout << "* Test Function" << std::endl;

	lua_State* L = luaL_newstate();
	luaL_openlibs(L);

	lua::set_global_fun(L, {
		{ "CSum", [](lua_State* L)->int { return lua::reg_fun(L, CSum); } },
		{ "CConcat", [](lua_State* L)->int { return lua::reg_fun(L, CConcat); } }
		});

	luaL_loadstring(L, R"***(
		local result = CSum(1, 2)
		print (result)

		local str = CConcat('Hello', 'World')
		print (str)
	)***");

	lua_pcall(L, 0, 0, 0);

	lua_close(L);
}

class CClass
{
public:
	int sum(int n1, int n2)
	{
		return n1 + n2;
	}

	std::string concat(std::string s1, std::string s2)
	{
		return s1 + s2;
	}
};

CClass* CGetClass()
{
	return new CClass();
}

template<>
struct ClassParam<CClass>
{
	static constexpr char lua_name[] = "CClass";

	inline static const std::vector<luaL_Reg> mem_funs = {
		{ "sum", [](lua_State* L)->int { return lua::reg_mem_fun(L, &CClass::sum); } },
		{ "concat", [](lua_State* L)->int { return lua::reg_mem_fun(L, &CClass::concat); } }
	};
};

void test_class()
{
	std::cout << "* Test Class" << std::endl;

	lua_State* L = luaL_newstate();
	luaL_openlibs(L);

	CClass* obj = new CClass();
	lua::Type<CClass>::push(L, obj);
	lua_setglobal(L, "CObj");

	lua::set_global_fun(L, { 
		{ "CGetClass", [](lua_State* L)->int { return lua::reg_fun(L, CGetClass); } },
	});

	luaL_loadstring(L, R"***(
		print '1'
		local obj = CGetClass()
		print '2'
		local r0 = obj:sum(5, 5)
		print '3'
		print (r0)
		print '4'

		local result = CObj:sum(1, 2)
		print (result)

		local str = CObj:concat('Hello', 'World')
		print (str)
	)***");

	lua_pcall(L, 0, 0, 0);

	lua_close(L);
}

int main()
{
	test_function();

	test_class();
}
