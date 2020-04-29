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

	try
	{
		lua::pcall(L);
	}
	catch (lua::error& err)
	{
		std::cerr << err.what() << std::endl;
	}

	auto luaVar = lua::get_global<int>(L, "luaVar");
	std::cout << "luaVar = " << luaVar << std::endl;

	auto luaFun = lua::lua_function<int, int>(L, "luaFun");
	try
	{
		int luaFunRes = luaFun(5);
		std::cout << "luaFun result = " << luaFunRes << std::endl;
	}
	catch (lua::error& err)
	{
		std::cerr << err.what() << std::endl;
	}

	auto luaFun2 = lua::lua_function<void, int>(L, "luaFun2");
	try
	{
		luaFun2(5);
	}
	catch (lua::error& err)
	{
		std::cerr << err.what() << std::endl;
	}

	lua_close(L);
}

class CClass
{
public:
	int getX() { return m_x; }
	void setX(int value) { m_x = value; }

	int sum(int n1, int n2)
	{
		return n1 + n2;
	}

	std::string concat(std::string s1, std::string s2)
	{
		return s1 + s2;
	}

protected:
	int m_x;
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
		{ "getX", [](lua_State* L)->int { return lua::invoke_mem_fun(L, &CClass::getX); } },
		{ "setX", [](lua_State* L)->int { return lua::invoke_mem_fun(L, &CClass::setX); } },
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

		obj:setX(100)
		print (obj:getX())

		local result = CObj:sum(1, 2)
		print (result)

		local str = CObj:concat('Hello', 'World')
		print (str)
	)***");

	try
	{
		lua::pcall(L);
	}
	catch (lua::error& err)
	{
		std::cerr << err.what() << std::endl;
	}

	lua_close(L);
}

struct color
{
	int r;
	int g;
	int b;
};


void test_table()
{
	std::cout << "* Test Table" << std::endl;

	lua_State* L = luaL_newstate();
	luaL_openlibs(L);

	luaL_loadstring(L, R"***(
		background = { r = 64, g = 128, b = 255 }
		
		player = { name = 'John Smith', score = 100 }
	)***");

	try
	{
		lua::pcall(L);
	}
	catch (lua::error& err)
	{
		std::cerr << err.what() << std::endl;
	}

	lua::Table<std::string> tab(L, "background");

	std::cout << "Acces by range based for loop" << std::endl;
	for (auto r : tab)
	{
		std::cout << r.get_key() << ": " << (int)r.get_value() << std::endl;
	}

	std::cout << "Access by key" << std::endl;
	std::cout << "r: " << (int)tab["r"] << std::endl;
	std::cout << "g: " << (int)tab["g"] << std::endl;
	std::cout << "b: " << (int)tab["b"] << std::endl;

	std::cout << "Contains key" << std::endl;
	std::cout << "tab['r']: " << tab.contains_key<int>("r") << std::endl;
	std::cout << "tab['foo']: " << tab.contains_key<int>("foo") << std::endl;

	lua_close(L);
}

int main()
{
	test_function();

	test_class();

	test_table();
}
