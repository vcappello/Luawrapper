#pragma once

#include "lua.hpp"
#include <string>
#include <vector>
#include <map>
#include <any>
#include <variant>
#include <stdexcept>

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
	class error : public std::runtime_error
	{
	public:
		error(const std::string& msg) : runtime_error(msg.c_str()) {}
		virtual ~error() noexcept {}
	};

	/*==================================================
	  state types

	  This section contain the definition of the class
	  Type 
	====================================================*/

	template<typename T, typename Enable = void>
	struct Type
	{
		static constexpr bool is_lua_type = false;

		static void push(lua_State* state, const T& value) {};
		static T get(lua_State* state, int index) { };
		static T check(lua_State* state, int arg) { };
		static bool type_check(lua_State* state, int index) { };
	};

	template<>
	struct Type<int>
	{
		static constexpr bool is_lua_type = true;

		static void push(lua_State* state, const int& value) { lua_pushinteger(state, value); }
		static int get(lua_State* state, int index) { return static_cast<int>(lua_tointeger(state, index)); }
		static int check(lua_State* state, int arg) { return static_cast<int>(luaL_checkinteger(state, arg)); }
		static bool type_check(lua_State* state, int index) { return lua_isinteger(state, index); }
	};

	template<>
	struct Type<double>
	{
		static constexpr bool is_lua_type = true;

		static void push(lua_State* state, const double& value) { lua_pushnumber(state, value); }
		static double get(lua_State* state, int index) { return lua_tonumber(state, index); }
		static double check(lua_State* state, int arg) { return luaL_checknumber(state, arg); }
		static bool type_check(lua_State* state, int index) { return lua_isnumber(state, index); }
	};

	template<>
	struct Type<bool>
	{
		static constexpr bool is_lua_type = true;

		static void push(lua_State* state, const bool& value) { lua_pushboolean(state, value); }
		static bool get(lua_State* state, int index) { return lua_toboolean(state, index); }
		static bool check(lua_State* state, int arg) { return lua_toboolean(state, arg); }
		static bool type_check(lua_State* state, int index) { return lua_isboolean(state, index); }
	};

	template<>
	struct Type<std::string>
	{
		static constexpr bool is_lua_type = true;

		static void push(lua_State* state, const std::string& value) { lua_pushstring(state, value.c_str()); }
		static std::string get(lua_State* state, int index) { return lua_tostring(state, index); }
		static std::string check(lua_State* state, int arg) { return luaL_checkstring(state, arg); }
		static bool type_check(lua_State* state, int index) { return lua_isboolean(state, index); }
	};

	/*==================================================
	  Table
	====================================================*/

	struct ValueProxy
	{
		lua_State* state;
		int index;

		ValueProxy(lua_State* state, int index) : state(state), index(index) {}

		template<typename T>
		operator T ()
		{
			T value = Type<T>::get(state, index);
			lua_pop(state, 1);
			return value;
		}
	};

	template<typename TKey>
	struct Table
	{
		lua_State* state;
		int index;

		Table(lua_State* state, const std::string& table_name) : state(state)
		{ 
			lua_getglobal(state, table_name.c_str());
			index = lua_gettop(state);
		}

		Table(lua_State* state, int index) : state(state), index(index)
		{
		}

		template<typename TKey>
		struct Pair
		{
			lua_State* state;

			Pair(lua_State* state) : state( state ) { }

			TKey get_key() { return Type<TKey>::get(state, -2); }

			ValueProxy get_value() { return ValueProxy(state, -1); }
		};

		struct Iterator
		{
			lua_State* state;
			size_t counter;
			int index;

			Iterator(lua_State* state, size_t counter, int index) : state(state), counter(counter), index( index ) { }

			Iterator operator++() 
			{ 
				// NOTE: Do not need to pop because the ValueProxy class
				// already pop last value
				// lua_pop(state, 1); 

				if (lua_next(state, index) != 0)
				{
					// Next
					++counter;
					return *this;
				}
				else
				{
					// Last
					counter = 0;
					return *this;
				}
			}

			bool operator!=(const Iterator& rhs) { return counter != rhs.counter; }
			const Pair<TKey>& operator*() const { return Pair<TKey>(state); }
		};

		Iterator begin() 
		{ 
			lua_pushnil(state);
			if (lua_next(state, index) != 0)
			{
				// Begin
				return Iterator(state, 1, index);
			}
			else
			{
				// End (empty table)
				return end();
			}
		};

		Iterator end() 
		{ 
			return Iterator(state, 0, 0);
		}

		ValueProxy operator[](TKey key)
		{
			Type<TKey>::push(state, key);
			lua_gettable(state, index); /* get table[key] */ 
			auto result = ValueProxy(state, -1);

			return result;
		}

		template<typename TValue>
		bool contains_key(TKey key)
		{
			Type<TKey>::push(state, key);
			lua_gettable(state, index); /* get table[key] */
			return Type<TValue>::type_check(state, -1);
		}

	};

	/*==================================================
	  User defined class types
	====================================================*/

	template<typename T>
	struct Type<T, std::enable_if_t<
				   std::is_class_v<T> || 
		           std::is_class_v<std::remove_pointer_t<T>> && 
		           !std::is_constructible_v<std::string, T>>>
	{
		static constexpr bool is_lua_type = true;

		using TPtr = std::conditional_t<std::is_class<T>::value, T*, T>;
		using TVal = std::conditional_t<std::is_class<T>::value, T, std::remove_pointer_t<T>>;

		static void push(lua_State* state, TPtr value) 
		{  
			if (luaL_newmetatable(state, ClassParam<TVal>::lua_name) == 1)
			{
				// Metatable index for objects
				lua_pushvalue(state, -1);
				lua_setfield(state, -2, "__index");
				// Set all methods
				for (auto r : ClassParam<TVal>::mem_funs)
				{
					lua_pushcfunction(state, r.func);
					lua_setfield(state, -2, r.name);
				}
				// Pop the metatable from the stack
				lua_pop(state, 1);
				// Put the object in the stack
				*reinterpret_cast<TPtr*>(lua_newuserdata(state, sizeof(TPtr))) = value;
				luaL_setmetatable(state, ClassParam<TVal>::lua_name);
			}
			else
			{
				*reinterpret_cast<TPtr*>(lua_newuserdata(state, sizeof(TPtr))) = value;
				luaL_getmetatable(state, ClassParam<TVal>::lua_name);
				lua_setmetatable(state, -2);
			}
		}

		static TPtr get(lua_State* state, int index) 
		{  
			return (*reinterpret_cast<TPtr*>(lua_touserdata(state, index)));
		}

		static TPtr check(lua_State* state, int arg) { return (*reinterpret_cast<TPtr*>(luaL_checkudata(state, arg, ClassParam<TVal>::lua_name))); }
		static TPtr type_check(lua_State* state, int arg) { return (*reinterpret_cast<TPtr*>(luaL_testudata(state, arg, ClassParam<TVal>::lua_name))); }
	};

	/*==================================================
	  Register C/C++ function

	  Usage:
		lua::set_global_fun(state, {
			{ "CSum", [](lua_State* state)->int { return state::invoke_fun(state, CSum); } },
			{ "CConcat", [](lua_State* state)->int { return state::invoke_fun(state, CConcat); } }
		});
	====================================================*/
	
	// Call a function returning void
	template <typename... TFunArgs, typename... TEvaluatedArgs>
	int invoke_fun(lua_State* state, int, void(*fn)(TFunArgs... args), void(*)(), TEvaluatedArgs... evaluated_args)
	{
		fn(std::forward<TEvaluatedArgs>(evaluated_args)...);
		return 0;
	}

	// Call a function returning a type
	template <typename TRetVal, typename... TFunArgs, typename... TEvaluatedArgs>
	int invoke_fun(lua_State* state, int, TRetVal(*fn)(TFunArgs... args), TRetVal(*)(), TEvaluatedArgs... evaluated_args) 
	{
		auto ret = fn(std::forward<TEvaluatedArgs>(evaluated_args)...);
		Type<TRetVal>::push(state, ret);
		return 1;
	}

	// Extract the nth argument and, if there are more arguments call itself, if this is the last argument go to
	// call the c function
	template <typename TRetVal, typename... TFunArgs, typename TArg0, typename... TOtherArgs, typename... TEvaluatedArgs>
	typename std::enable_if<Type<TArg0>::is_lua_type, int>::type
		invoke_fun(lua_State* state, int i, TRetVal(*fn)(TFunArgs... args), TRetVal(*)(TArg0, TOtherArgs... other_values), TEvaluatedArgs... evaluated_args) 
	{
		TRetVal(*other_fn)(TOtherArgs... other_values) = nullptr;
		auto val = Type<TArg0>::get(state, i);
		return invoke_fun(state, i + 1, fn, other_fn, std::forward<TEvaluatedArgs>(evaluated_args)..., val);
	}

	// Helper function, entry point
	// Call like: invoke_fun(state, global_func);
	template <typename TRetVal, typename... TFunArgs>
	int invoke_fun(lua_State* state, TRetVal(*fn)(TFunArgs... args)) 
	{
		return invoke_fun(state, 1, fn, fn);
	}

	void set_global_fun(lua_State* state, std::vector<luaL_Reg> regs)
	{
		for (auto r : regs)
		{
			lua_pushcfunction(state, r.func);
			lua_setglobal(state, r.name);
		}
	}

	/*==================================================
	  Register C/C++ member function

	  Usage:
		invoke_mem_fun(state, &my_class::my_method);
	====================================================*/

	// Call member function returning void
	template <typename TObject, typename... TFunArgs, typename... TEvaluatedArgs>
	int invoke_mem_fun(lua_State* state, int, TObject* obj_ptr, void(TObject::* fn)(TFunArgs... args), void(TObject::*)(), TEvaluatedArgs... evaluated_args) {
		(obj_ptr->*fn)(std::forward<TEvaluatedArgs>(evaluated_args)...);
		return 0;
	}

	// Call member function returning a type
	template <typename TObject, typename TRetVal, typename... TFunArgs, typename... TEvaluatedArgs>
	int invoke_mem_fun(lua_State* state, int, TObject* obj_ptr, TRetVal(TObject::* fn)(TFunArgs... args), TRetVal(TObject::*)(), TEvaluatedArgs... evaluated_args) {
		auto ret = (obj_ptr->*fn)(std::forward<TEvaluatedArgs>(evaluated_args)...);
		Type<TRetVal>::push(state, ret);
		return 1;
	}

	// Extract the nth argument and, if there are more arguments call itself, if this is the last argument go to
	// call the c function
	template <typename TObject, typename TRetVal, typename... TFunArgs, typename TArg0, typename... TOtherArgs, typename... TEvaluatedArgs>
	typename std::enable_if<Type<TArg0>::is_lua_type, int>::type
		invoke_mem_fun(lua_State* state, int i, TObject* obj_ptr, TRetVal(TObject::* fn)(TFunArgs... args), TRetVal(TObject::*)(TArg0, TOtherArgs... other_values), TEvaluatedArgs... evaluated_args) {
		TRetVal(TObject:: * other_fn)(TOtherArgs... other_values) = nullptr;
		auto val = Type<TArg0>::get(state, i);
		return invoke_mem_fun(state, i + 1, obj_ptr, fn, other_fn, std::forward<TEvaluatedArgs>(evaluated_args)..., val);
	}

	// Helper function, entry point
	// Call like: invoke_fun(state, global_func);
	template <typename TObject, typename TRetVal, typename... TFunArgs>
	int invoke_mem_fun(lua_State* state, TRetVal(TObject::* fn)(TFunArgs... args)) {
		auto obj_ptr = Type<TObject>::check(state, 1);
		return invoke_mem_fun(state, 2, obj_ptr, fn, fn);
	}

	/*==================================================
	  Set global value
	====================================================*/
	template<typename T>
	std::enable_if_t<!std::is_invocable_r_v<int, T, lua_State*>>
	set_global(lua_State* state, const char* name, T value)
	{
		Type<T>::push(state, value);
		lua_setglobal(state, name);
	}

	template <typename T>
	std::enable_if_t<std::is_invocable_r_v<int, T, lua_State*>>
	set_global(lua_State* state, const char* name, T fn)
	{
		lua_pushcfunction(state, fn);
		lua_setglobal(state, name);
	}

	/*==================================================
	  Script and function call with error management
	====================================================*/
	int pcall(lua_State* state, int nargs = 0, int nresults = 0, int msgh = 0)
	{
		int result = 0;
		if (result = lua_pcall(state, nargs, nresults, msgh) != 0)
		{
			auto msg = Type<std::string>::get(state, -1);
			lua_pop(state, 1);
			throw error(msg);
		}

		return result;
	}

	/*==================================================
	  Get global value
	====================================================*/
	template<typename TRet, typename... TArgs>
	struct lua_function
	{
		lua_function(lua_State* state, const std::string& name) : state( state ), name( name ) { }

		TRet operator()(TArgs... args)
		{
			lua_getglobal(state, name.c_str());

			(Type<TArgs>::push(state, args), ...);

			pcall(state, sizeof...(TArgs), 1, 0);

			return Type<TRet>::get(state, -1);
		}

	protected:
		std::string name;
		lua_State* state;
	};

	template<typename... TArgs>
	struct lua_function<void, TArgs...>
	{
		lua_function(lua_State* state, const std::string& name) : state(state), name(name) { }

		void operator()(TArgs... args)
		{
			lua_getglobal(state, name.c_str());

			(Type<TArgs>::push(state, args), ...);

			pcall(state, sizeof...(TArgs), 0, 0);
		}

	protected:
		std::string name;
		lua_State* state;
	};

	template<typename T>
	T get_global(lua_State* state, const char* name)
	{
		lua_getglobal(state, name);
		return Type<T>::get(state, 1);
	}

}
