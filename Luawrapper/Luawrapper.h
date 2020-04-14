#pragma once

#include "lua.hpp"
#include <string>
#include <vector>

// ClassParam is declared in the global namespace in order to
// allow specialization anywhere
template<typename TClass>
struct ClassParam
{
	static constexpr char lua_name[] = "NONAME";

	static std::vector<luaL_Reg> mem_funs;
};

namespace lua
{
	/*==================================================
	  LUA types

	  This section contain the definition of the class
	  Type 
	====================================================*/

	template<typename T, typename Enable = void>
	struct Type
	{
		static constexpr bool is_lua_type = false;

		static void push(lua_State* lua, const T& value) {};
		static T get(lua_State* lua, int index) { };
		static T check(lua_State* lua, int arg) { };
		static bool type_check(lua_State* lua, int index) { };
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

	template<typename T>
	struct Type<T, std::enable_if_t<std::is_class<T>::value || std::is_class<std::remove_pointer_t<T>>::value>>
	{
		static constexpr bool is_lua_type = true;

		using TPtr = std::conditional_t<std::is_class<T>::value, T*, T>;
		using TVal = std::conditional_t<std::is_class<T>::value, T, std::remove_pointer_t<T>>;

		static void push(lua_State* lua, TPtr value) 
		{  
			if (luaL_newmetatable(lua, ClassParam<TVal>::lua_name) == 1)
			{
				// Metatable index for objects
				lua_pushvalue(lua, -1);
				lua_setfield(lua, -2, "__index");
				// Set all methods
				for (auto r : ClassParam<TVal>::mem_funs)
				{
					lua_pushcfunction(lua, r.func);
					lua_setfield(lua, -2, r.name);
				}
				// Pop the metatable from the stack
				lua_pop(lua, 1);
				// Put the object in the stack
				*reinterpret_cast<TPtr*>(lua_newuserdata(lua, sizeof(TPtr))) = value;
				luaL_setmetatable(lua, ClassParam<TVal>::lua_name);
			}
			else
			{
				*reinterpret_cast<TPtr*>(lua_newuserdata(lua, sizeof(TPtr))) = value;
				luaL_getmetatable(lua, ClassParam<TVal>::lua_name);
				lua_setmetatable(lua, -2);
			}
		}

		static TPtr get(lua_State* lua, int index) 
		{  
			return (*reinterpret_cast<TPtr*>(lua_touserdata(lua, index)));
		}

		static TPtr check(lua_State* lua, int arg) { return (*reinterpret_cast<TPtr*>(luaL_checkudata(lua, arg, ClassParam<TVal>::lua_name))); }
		static TPtr type_check(lua_State* lua, int arg) { return (*reinterpret_cast<TPtr*>(luaL_testudata(lua, arg, ClassParam<TVal>::lua_name))); }
	};

	/*==================================================
	  Register C/C++ function

	  Usage:
		lua::set_global_fun(lua, {
			{ "CSum", [](lua_State* lua)->int { return lua::invoke_fun(lua, CSum); } },
			{ "CConcat", [](lua_State* lua)->int { return lua::invoke_fun(lua, CConcat); } }
		});
	====================================================*/

	// At the end call the C function
	template <typename TRetVal, typename... TFunArgs, typename... TEvaluatedArgs>
	int invoke_fun(lua_State* lua, int, TRetVal(*fn)(TFunArgs... args), TRetVal(*)(), TEvaluatedArgs... evaluated_args) 
	{
		auto ret = fn(std::forward<TEvaluatedArgs>(evaluated_args)...);
		Type<TRetVal>::push(lua, ret);
		return 1;
	}

	// Extract the nth argument and, if there are more arguments call itself, if this is the last argument go to
	// call the c function
	template <typename TRetVal, typename... TFunArgs, typename TArg0, typename... TOtherArgs, typename... TEvaluatedArgs>
	typename std::enable_if<Type<TArg0>::is_lua_type, int>::type
		invoke_fun(lua_State* lua, int i, TRetVal(*fn)(TFunArgs... args), TRetVal(*)(TArg0, TOtherArgs... other_values), TEvaluatedArgs... evaluated_args) 
	{
		TRetVal(*other_fn)(TOtherArgs... other_values) = nullptr;
		auto val = Type<TArg0>::get(lua, i);
		return invoke_fun(lua, i + 1, fn, other_fn, std::forward<TEvaluatedArgs>(evaluated_args)..., val);
	}

	// Helper function, entry point
	// Call like: invoke_fun(lua, global_func);
	template <typename TRetVal, typename... TFunArgs>
	int invoke_fun(lua_State* lua, TRetVal(*fn)(TFunArgs... args)) 
	{
		return invoke_fun(lua, 1, fn, fn);
	}

	void set_global_fun(lua_State* lua, std::vector<luaL_Reg> regs)
	{
		for (auto r : regs)
		{
			lua_pushcfunction(lua, r.func);
			lua_setglobal(lua, r.name);
		}
	}

	/*==================================================
	  Register C/C++ member function

	  Usage:
		invoke_mem_fun(lua, &my_class::my_method);
	====================================================*/

	// At the end call the C function
	template <typename TObject, typename TRetVal, typename... TFunArgs, typename... TEvaluatedArgs>
	int invoke_mem_fun(lua_State* lua, int, TObject* obj_ptr, TRetVal(TObject::* fn)(TFunArgs... args), TRetVal(TObject::*)(), TEvaluatedArgs... evaluated_args) {
		auto ret = (obj_ptr->*fn)(std::forward<TEvaluatedArgs>(evaluated_args)...);
		Type<TRetVal>::push(lua, ret);
		return 1;
	}

	// Extract the nth argument and, if there are more arguments call itself, if this is the last argument go to
	// call the c function
	template <typename TObject, typename TRetVal, typename... TFunArgs, typename TArg0, typename... TOtherArgs, typename... TEvaluatedArgs>
	typename std::enable_if<Type<TArg0>::is_lua_type, int>::type
		invoke_mem_fun(lua_State* lua, int i, TObject* obj_ptr, TRetVal(TObject::* fn)(TFunArgs... args), TRetVal(TObject::*)(TArg0, TOtherArgs... other_values), TEvaluatedArgs... evaluated_args) {
		TRetVal(TObject:: * other_fn)(TOtherArgs... other_values) = nullptr;
		auto val = Type<TArg0>::get(lua, i);
		return invoke_mem_fun(lua, i + 1, obj_ptr, fn, other_fn, std::forward<TEvaluatedArgs>(evaluated_args)..., val);
	}

	// Helper function, entry point
	// Call like: invoke_fun(lua, global_func);
	template <typename TObject, typename TRetVal, typename... TFunArgs>
	int invoke_mem_fun(lua_State* lua, TRetVal(TObject::* fn)(TFunArgs... args)) {
		auto obj_ptr = Type<TObject>::check(lua, 1);
		return invoke_mem_fun(lua, 2, obj_ptr, fn, fn);
	}

	/*==================================================
	  Set global value
	====================================================*/
	template<typename T>
	std::enable_if_t<!std::is_invocable_r_v<int, T, lua_State*>>
	set_global(lua_State* lua, const char* name, T value)
	{
		Type<T>::push(lua, value);
		lua_setglobal(lua, name);
	}

	template <typename T>
	std::enable_if_t<std::is_invocable_r_v<int, T, lua_State*>>
	set_global(lua_State* lua, const char* name, T fn)
	{
		lua_pushcfunction(lua, fn);
		lua_setglobal(lua, name);
	}
}
