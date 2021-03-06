#pragma comment(lib, "Irrlicht.lib")
#ifdef _DEBUG
#pragma comment(lib, "LuaLibd.lib")
#else
#pragma comment(lib, "Lualib.lib")
#endif

#include <lua.hpp>
#include <Windows.h>
#include <iostream>
#include <thread>
#include "lua.hpp"
//#include <irrlicht.h>
#include <SFML/Graphics.hpp>
#include "RealCode/Player.h"
#include "RealCode/Enemy.h"
#include "RealCode/GameHandler.h"
#include "RealCode/PublicLuaFunctions/LuaMath.h"

void ConsoleThread(lua_State* L) {
	char command[1000];
	while(GetConsoleWindow()) {
		memset(command, 0, 1000);
		std::cin.getline(command, 1000);
		if (luaL_loadstring(L, command) || lua_pcall(L, 0, 0, 0)) {
			std::cout << lua_tostring(L, -1) << '\n';
			lua_pop(L, 1);
		}
	}
}

static int test(lua_State * L)
{
	return 0;
}


const int screenWidht = 420;
const int screenHight = 420;

sf::Clock deltaClock;
sf::Time dt;
int main()
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	using namespace std::chrono_literals;
	lua_State* L = luaL_newstate();
	luaL_openlibs(L);
	
	sf::RenderWindow window(sf::VideoMode(screenWidht, screenHight), "SFML works!");
	Map * map = new Map(&window, L);
	GameHandler gameHandle(L, &window, map);

	lua_pushnumber(L, screenWidht);
	lua_setglobal(L, "SCREEN_WIDTH");
	
	lua_pushnumber(L, screenHight);
	lua_setglobal(L, "SCREEN_HEIGHT");

	LuaMath::Math pushLuaMath;
	pushLuaMath.pushLuaFunctions(L);

	int highScore = 0;
	std::ifstream file;
	file.open("Highscore.txt");
	file >> highScore;
	file.close();

	gameHandle.setHighScore(highScore);
	int error = luaL_loadfile(L, "Lua/Start.lua") || lua_pcall(L, 0, 0, 0);
	if (error)
	{
		std::cout << lua_tostring(L, -1) << '\n';
		lua_pop(L, 1);
	}
	error = luaL_loadfile(L, "Lua/Test.lua") || lua_pcall(L, 0, 0, 0);
	if (error)
	{
		std::cout << lua_tostring(L, -1) << '\n';
		lua_pop(L, 1);
	}

	luaL_dofile(L, "Lua/Test.lua");
	std::thread conThread(ConsoleThread, L);
	bool keepAlive = true;
	while (window.isOpen() && keepAlive)
	{
		dt = deltaClock.restart();
		lua_pushnumber(L, dt.asSeconds() * 50);
		lua_setglobal(L, "DELTA_TIME");

		
		lua_getglobal(L, "update");
		lua_pushnumber(L, dt.asSeconds());
		lua_call(L, 1, 0);

		sf::Event event;
		while (window.pollEvent(event))
		{
			if (event.type == sf::Event::Closed)
				window.close();
		}

		gameHandle.Update(L,dt.asSeconds() * 50);
		

		window.clear();
		window.draw(gameHandle);
		
		window.display();
		
	}
	delete map;
	//delete L;
	conThread.join();

	return 0;
}