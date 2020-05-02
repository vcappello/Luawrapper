# Luawrapper

A C++17 basic, header only, lightweight, non intrusive Lua wrapper. 
This library contains only a set of helper for simplify
the binding of variables, functions and classes to Lua, it is not a full Lua wrapper.
The library consists in a single header file, to use it simply include *Luawrapper.h*
in your project.
For bind C++ classes you don't need to add extra-code to your classes.

## Bind variables
Share a C++ variable with Lua:

```c++
int c_var = 5;
lua::set_global(L, "LuaVar", c_var);
```

Read a Lua variable from C++:
```c++
auto lua_var = lua::get_global<int>(L, "LuaVar");
```

Recognized types:

Lua type  | C++ type
----------|----------------------
Boolean   | bool
Integer   | int
Number    | double
String    | std::string or char*

## Bind functions
Arguments and return types are the same defined in *Bind variables*.
### Share a C++ function with Lua
Suppose this is the C++ function:
```c++
// Function called by Lua
int CSum(int n1, int n2)
{
	return n1 + n2;
}
```
Register the function in this way:
```c++
// Register the C++ function, before lua::pcall
int register_function()
{
    lua::set_global(L, "CSum", [](lua_State* L)->int { return lua::invoke_fun(L, CSum); });
}
```

And you can call from Lua like:
```lua
local res = CSum(1, 2) -- Call C++ function 
```

### Call a Lua function from C++
Suppose this is the Lua function:
```lua
function LuaSum(a,b)
    return a + b
end
```
Register the function in this way, need to give the return type and the type of each argument:
```c++
auto lua_sum = lua::lua_function<int, int, int>(L, "LuaSum");
```
Call the Lua function in this way, like a C++ function:

```c++
int result = lua_sum(1, 2);
```

## Bind Classes


