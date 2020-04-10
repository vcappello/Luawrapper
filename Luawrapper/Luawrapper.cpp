// Luawrapper.cpp : Questo file contiene la funzione 'main', in cui inizia e termina l'esecuzione del programma.
//

#include <iostream>

#include "Luawrapper.h"

int CSum(int n1, int n2)
{
	std::cout << "[CSum (" << n1 << ", " << n2 << ")]" << std::endl;
	return n1 + n2;
}

std::string CConcat(std::string s1, std::string s2)
{
	return s1 + s2;
}

void test_reg_fun()
{
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

int main()
{
	test_reg_fun();
}
