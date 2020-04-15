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
		{ "CSum", [](lua_State* L)->int { return lua::invoke_fun(L, CSum); } },
		{ "CConcat", [](lua_State* L)->int { return lua::invoke_fun(L, CConcat); } }
		});

	luaL_loadstring(L, R"***(
		local result = CSum(1, 2)
		print (result)

		local str = CConcat('Hello', 'World')
		print (str)

		luaVar = 55

		function luaFun(a)
			return a + 1
		end

		function luaFun2(a)
			print 'luaFun2'
		end
	)***");

	lua_pcall(L, 0, 0, 0);

	auto luaVar = lua::get_global<int>(L, "luaVar");
	std::cout << "luaVar = " << luaVar << std::endl;

	auto luaFun = lua::lua_function<int, int>(L, "luaFun");
	int luaFunRes = luaFun(5);
	std::cout << "luaFun result = " << luaFunRes << std::endl;

	auto luaFun2 = lua::lua_function<void, int>(L, "luaFun2");
	luaFun2(5);

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
		{ "sum", [](lua_State* L)->int { return lua::invoke_mem_fun(L, &CClass::sum); } },
		{ "concat", [](lua_State* L)->int { return lua::invoke_mem_fun(L, &CClass::concat); } }
	};
};

void test_class()
{
	std::cout << "* Test Class" << std::endl;

	lua_State* L = luaL_newstate();
	luaL_openlibs(L);

	CClass* obj = new CClass();
	lua::set_global(L, "CObj", obj);

	lua::set_global(L, "CGetClass", [](lua_State* L)->int { return lua::invoke_fun(L, CGetClass); });

	luaL_loadstring(L, R"***(
		local obj = CGetClass()
		local r0 = obj:sum(5, 5)
		print (r0)

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
