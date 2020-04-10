#pragma once

#include "lua.hpp"
#include <string>
#include <vector>

namespace lua
{
	/*==================================================
	  LUA types
	====================================================*/

	template<typename T, typename Enable = void>
	struct Type
	{
		static constexpr bool is_lua_type = false;

		static void push(lua_State* lua, const T& value) {};
		static T get(lua_State* lua, int index) { return T{}; };
		static T check(lua_State* lua, int arg) { return T{}; };
		static bool type_check(lua_State* lua, int index) { return false; };
	};

	template<>
	struct Type<int>
	{
		static constexpr bool is_lua_type = true;

		static void push(lua_State* lua, const int& value) { lua_pushinteger(lua, value); }
		static int get(lua_State* lua, int index) { return lua_tointeger(lua, index); }
		static int check(lua_State* lua, int arg) { return luaL_checkinteger(lua, arg); }
		static bool type_check(lua_State* lua, int index) { return lua_isinteger(lua, index); }
	};

	template<>
	struct Type<double>
	{
		static constexpr bool is_lua_type = true;

		static void push(lua_State* lua, const double& value) { lua_pushnumber(lua, value); }
		static double get(lua_State* lua, int index) { return lua_tonumber(lua, index); }
		static double check(lua_State* lua, int arg) { return luaL_checknumber(lua, arg); }
		static bool type_check(lua_State* lua, int index) { return lua_isnumber(lua, index); }
	};

	template<>
	struct Type<bool>
	{
		static constexpr bool is_lua_type = true;

		static void push(lua_State* lua, const bool& value) { lua_pushboolean(lua, value); }
		static bool get(lua_State* lua, int index) { return lua_toboolean(lua, index); }
		static bool check(lua_State* lua, int arg) { return lua_toboolean(lua, arg); }
		static bool type_check(lua_State* lua, int index) { return lua_isboolean(lua, index); }
	};

	template<>
	struct Type<std::string>
	{
		static constexpr bool is_lua_type = true;

		static void push(lua_State* lua, const std::string& value) { lua_pushstring(lua, value.c_str()); }
		static std::string get(lua_State* lua, int index) { return lua_tostring(lua, index); }
		static std::string check(lua_State* lua, int arg) { return luaL_checkstring(lua, arg); }
		static bool type_check(lua_State* lua, int index) { return lua_isboolean(lua, index); }
	};

	/*==================================================
	  User defined class types
	====================================================*/

	template<typename TClass>
	struct ClassParam
	{
		static constexpr char lua_name[] = nullptr;

		static std::vector<luaL_Reg> mem_funs;
	};

	template<typename T>
	struct Type<T, std::enable_if_t<std::is_class<T>::value>>
	{
		static constexpr bool is_lua_type = true;

		static void push(lua_State* lua, T* value) 
		{  
			// Create a new metatable
			luaL_newmetatable(lua, ClassParam<T>::lua_name);
			// Metatable index for objects
			lua_pushvalue(lua, -1);
			lua_setfield(lua, -2, "__index");
			// Set all methods
			for (auto r : ClassParam<T>::mem_funs)
			{
				lua_pushcfunction(lua, r.func);
				lua_setfield(lua, -2, r.name);
			}
			// Pop the metatable from the stack
			lua_pop(lua, 1);
			// Put the object in the stack
			*reinterpret_cast<T**>(lua_newuserdata(lua, sizeof(T*))) = value;
			luaL_setmetatable(lua, ClassParam<T>::lua_name);
		}

		//static T* get(lua_State* lua, int index) { return ...; }
		static T* check(lua_State* lua, int arg) { return (*reinterpret_cast<T**>(luaL_checkudata(lua, arg, ClassParam<T>::lua_name))); }
		static T* type_check(lua_State* lua, int arg) { return (*reinterpret_cast<T**>(luaL_testudata(lua, arg, ClassParam<T>::lua_name))); }
	};

	/*==================================================
	  Register C/C++ function

	  Usage:
		lua::set_global_fun(L, {
			{ "CSum", [](lua_State* L)->int { return lua::reg_fun(L, CSum); } },
			{ "CConcat", [](lua_State* L)->int { return lua::reg_fun(L, CConcat); } }
		});
	====================================================*/

	// At the end call the C function
	template <typename TRetVal, typename... TFunArgs, typename... TEvaluatedArgs>
	int reg_fun(lua_State* L, int, TRetVal(*fn)(TFunArgs... args), TRetVal(*)(), TEvaluatedArgs... evaluated_args) 
	{
		auto ret = fn(std::forward<TEvaluatedArgs>(evaluated_args)...);
		Type<TRetVal>::push(L, ret);
		return 1;
	}

	// Extract the nth argument and, if there are more arguments call itself, if this is the last argument go to
	// call the c function
	template <typename TRetVal, typename... TFunArgs, typename TArg0, typename... TOtherArgs, typename... TEvaluatedArgs>
	typename std::enable_if<Type<TArg0>::is_lua_type, int>::type
		reg_fun(lua_State* L, int i, TRetVal(*fn)(TFunArgs... args), TRetVal(*)(TArg0, TOtherArgs... other_values), TEvaluatedArgs... evaluated_args) 
	{
		TRetVal(*other_fn)(TOtherArgs... other_values) = nullptr;
		auto val = Type<TArg0>::get(L, i);
		return reg_fun(L, i + 1, fn, other_fn, std::forward<TEvaluatedArgs>(evaluated_args)..., val);
	}

	// Helper function, entry point
	// Call like: reg_fun(L, global_func);
	template <typename TRetVal, typename... TFunArgs>
	int reg_fun(lua_State* L, TRetVal(*fn)(TFunArgs... args)) 
	{
		return reg_fun(L, 1, fn, fn);
	}

	void set_global_fun(lua_State* L, std::vector<luaL_Reg> regs)
	{
		for (auto r : regs)
		{
			lua_pushcfunction(L, r.func);
			lua_setglobal(L, r.name);
		}
	}

	/*==================================================
	  Register C/C++ member function
	  Usage:
		reg_mem_fun(L, &my_class::my_method);
	====================================================*/

	// At the end call the C function
	template <typename TObject, typename TRetVal, typename... TFunArgs, typename... TEvaluatedArgs>
	int reg_mem_fun(lua_State* L, int, TObject* obj_ptr, TRetVal(TObject::* fn)(TFunArgs... args), TRetVal(TObject::*)(), TEvaluatedArgs... evaluated_args) {
		auto ret = (obj_ptr->*fn)(std::forward<TEvaluatedArgs>(evaluated_args)...);
		Type<TRetVal>::push(L, ret);
		return 1;
	}

	// Extract the nth argument and, if there are more arguments call itself, if this is the last argument go to
	// call the c function
	template <typename TObject, typename TRetVal, typename... TFunArgs, typename TArg0, typename... TOtherArgs, typename... TEvaluatedArgs>
	typename std::enable_if<Type<TArg0>::is_lua_type, int>::type
		reg_mem_fun(lua_State* L, int i, TObject* obj_ptr, TRetVal(TObject::* fn)(TFunArgs... args), TRetVal(TObject::*)(TArg0, TOtherArgs... other_values), TEvaluatedArgs... evaluated_args) {
		TRetVal(TObject:: * other_fn)(TOtherArgs... other_values) = nullptr;
		auto val = Type<TArg0>::get(L, i);
		return reg_mem_fun(L, i + 1, obj_ptr, fn, other_fn, std::forward<TEvaluatedArgs>(evaluated_args)..., val);
	}

	// Helper function, entry point
	// Call like: reg_fun(L, global_func);
	template <typename TObject, typename TRetVal, typename... TFunArgs>
	int reg_mem_fun(lua_State* L, TRetVal(TObject::* fn)(TFunArgs... args)) {
		auto obj_ptr = Type<TObject>::check(L, 1);
		return reg_mem_fun(L, 2, obj_ptr, fn, fn);
	}

}
