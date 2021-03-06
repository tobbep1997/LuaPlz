#include "GameHandler.h"
#include <stdlib.h>
#include <time.h>

#define DEBUG 0

GameHandler::GameHandler(lua_State* L, sf::RenderWindow* window, Map * map)
{
	player = new Player(L, window->getSize().x / 2 , window->getSize().y / 2);

	PushLuaFunctions(L);

	enemyList.push_back(new Enemy(L, 100, 100));
	enemyList.push_back(new Enemy(L, 1, 100));
	wndPtr = window;
	bh = BulletHandler(window->getSize());
	bh.pushToLua(L);

	m_map = map;
	srand(time(NULL));

	if (!font.loadFromFile("arial.ttf"))
	{
		std::cout << "Font went to shitters" << std::endl;
	}

	text.setFont(font);
	text.setFillColor(sf::Color::White);
	text.setCharacterSize(25);
	text.setPosition(0, window->getSize().y-30);

	hpText.setFont(font);
	hpText.setFillColor(sf::Color::White);
	hpText.setCharacterSize(25);
	hpText.setPosition(window->getSize().x - 95, window->getSize().y - 30);

	deadText.setFont(font);
	deadText.setFillColor(sf::Color::Red);
	deadText.setCharacterSize(60);
	deadText.setPosition(20, window->getSize().y /2);
	deadText.setString("YOU DEAD");

	scoreText.setFont(font);
	scoreText.setFillColor(sf::Color::White);
	scoreText.setCharacterSize(25);
	scoreText.setString("Score: " + std::to_string(score));

	highScoreText.setFont(font);
	highScoreText.setFillColor(sf::Color::Red);
	highScoreText.setCharacterSize(25);
	highScoreText.setPosition(20, window->getSize().y / 6);
	
}
	

GameHandler::~GameHandler()
{
	delete player;
	delete enemy;
	//delete m_map;
	for (size_t i = 0; i < enemyList.size(); i++)
	{
		if (enemyList.at(i))
		{
			delete enemyList.at(i);
		}
	}
	enemyList.clear();
}

void GameHandler::Update(lua_State* L, const float deltaTime)
{
	m_map->update();
	_playerInputHandler(L);
	int error = luaL_loadfile(L, "Lua/GameHandler.lua") ||lua_pcall(L, 0, 0, 0);
	if (error)
	{
		std::cout << lua_tostring(L, -1) << '\n';
		lua_pop(L, 1);
	}

	error = luaL_loadfile(L, "Lua/EnemyHandler.lua") || lua_pcall(L, 0, 0, 0);
	if (error)
	{
		std::cout << lua_tostring(L, -1) << '\n';
		lua_pop(L, 1);
	}

	for (size_t i = 0; i < enemyList.size(); i++)
	{
		if (enemyList.at(i)->getExploded() == false)
		{
			enemyList.at(i)->Update(L);
		}
	}

	for (size_t i = 0; i < enemyList.size(); i++)
	{
		if (enemyList.at(i)->getExploded())
		{
			sf::CircleShape temp;
			temp.setFillColor(sf::Color(255,0,0,125));
			temp.setRadius(20.0f);
			temp.setPosition(enemyList.at(i)->getPosition());
			//deadEnemys.push_back(temp);

			delete enemyList.at(i);
			enemyList.erase(enemyList.begin() + i);
			break;
		}
	}
	
	for (size_t i = 0; i < m_map->getTiles().size(); i++)
	{
		for (size_t j = 0; j < enemyList.size(); j++)
		{
			if (m_map->getTiles().at(i)->getGlobalBounds().intersects(enemyList.at(j)->getShape().getGlobalBounds()))
			{
				sf::Vector2f temp = enemyList.at(j)->getPosition();
				temp.x = rand() % 20 + temp.x - 10;
				temp.y = rand() % 20 + temp.y - 10;
				enemyList.at(j)->setPosition(temp);
			}
		}
	}

	/*for (size_t i = 0; i < deadEnemys.size(); i++)
	{
		if (deadEnemys.at(i).getFillColor().a > 10)
		{
			sf::Color temp = deadEnemys.at(i).getFillColor();
			temp.a = temp.a - 0.001f;
			deadEnemys.at(i).setFillColor(temp);
		}
	}*/
	using namespace std::chrono_literals;

	if (m_i == 0)
	{
		m_i++;
		fadeThread = std::async(std::launch::async, &GameHandler::_enemyFade, this);
	}
	auto status = fadeThread.wait_for(0ms);

	if (status == std::future_status::ready) {
		fadeThread.get();
		fadeThread = std::async(std::launch::async, &GameHandler::_enemyFade, this);

	}
	else {
		//std::cout << "Thread running" << std::endl;
	}
	bh.update(deltaTime,enemyList, m_map->getTiles());

	if (player->getHealth() <= 0)
	{
		if (!firstTime)
		{
			keepAlive = false;
			if (highScore < score)
			{
				std::ofstream file;
				file.open("Highscore.txt");
				file << score;
				file.close();
				highScore = score;
				
			}
		}
		highScoreText.setString("Highscore: " + std::to_string(highScore) + "\nYour Score: " + std::to_string(score));
		firstTime = false;
	}
}

bool GameHandler::getAlive()
{
	return keepAlive;
}

void GameHandler::setHighScore(int highScore)
{
	this->highScore = highScore;
}

void GameHandler::PushLuaFunctions(lua_State * L)
{
	lua_pushlightuserdata(L, this);
	lua_pushcclosure(L, GameHandler::luaAddEnemy, 1);
	lua_setglobal(L, "AddEnemy");

	lua_pushlightuserdata(L, this);
	lua_pushcclosure(L, GameHandler::random, 1);
	lua_setglobal(L, "Rand");

	lua_pushlightuserdata(L, this);
	lua_pushcclosure(L, GameHandler::luaSetTextBullet, 1);
	lua_setglobal(L, "SetBulletText");

	lua_pushlightuserdata(L, this);
	lua_pushcclosure(L, GameHandler::luaSetTextScore, 1);
	lua_setglobal(L, "SetScoreText");
}

void GameHandler::AddingEnemy(double posX, double posY)
{
	enemyList.push_back(new Enemy(posX, posY));
}

int GameHandler::luaAddEnemy(lua_State * L)
{
	if (lua_isnumber(L, -1) && lua_isnumber(L, -2))
	{
		GameHandler* p = static_cast<GameHandler*>(lua_touserdata(L, lua_upvalueindex(1)));
		p->AddingEnemy(lua_tonumber(L, -2), lua_tonumber(L, -1));
		lua_pop(L, 2);

		return 0;
	}
	else
	{
		std::cout << "Error: Expected EnemyAddingEnemy(double, double)" << std::endl;
	}
	return 0;
}

int GameHandler::luaSetTextBullet(lua_State * L)
{
	if (lua_isnumber(L, -1) && lua_isnumber(L, -2))
	{
		GameHandler* p = static_cast<GameHandler*>(lua_touserdata(L, lua_upvalueindex(1)));
		p->SetBulletText(lua_tonumber(L, -2), lua_tonumber(L, -1));
		lua_pop(L, 2);

		return 0;
	}
	else
	{
		std::cout << "Error: Expected SetBulletText(double)" << std::endl;
	}
	return 0;
}

void GameHandler::SetBulletText(int bullets, int hp)
{
	text.setString(std::to_string(bullets) + "/60");
	hpText.setString("HP: " + std::to_string(hp));
}

int GameHandler::luaSetTextScore(lua_State * L)
{
	if (lua_isnumber(L, -1))
	{
		GameHandler* p = static_cast<GameHandler*>(lua_touserdata(L, lua_upvalueindex(1)));
		p->SetScoreText(lua_tonumber(L, -1));
		lua_pop(L, 1);

		return 0;
	}
	else
	{
		std::cout << "Error: Expected SetSCoreText(double)" << std::endl;
	}
	return 0;
}

void GameHandler::SetScoreText(int amount)
{
	score += amount;
	scoreText.setString("Score: " + std::to_string(score));
}

void GameHandler::_playerInputHandler(lua_State* L)
{
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::A))
	{
		lua_pushstring(L, "A");
		lua_setglobal(L, "KeyBoardState");
	}
	else if (sf::Keyboard::isKeyPressed(sf::Keyboard::D))
	{
		lua_pushstring(L, "D");
		lua_setglobal(L, "KeyBoardState");
	}
	else if (sf::Keyboard::isKeyPressed(sf::Keyboard::W))
	{
		lua_pushstring(L, "W");
		lua_setglobal(L, "KeyBoardState");
	}
	else if (sf::Keyboard::isKeyPressed(sf::Keyboard::S))
	{
		lua_pushstring(L, "S");
		lua_setglobal(L, "KeyBoardState");
	}
	else
	{
		lua_pushstring(L, "FUCK");
		lua_setglobal(L, "KeyBoardState");
	}
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::R))
	{
		lua_pushstring(L, "R");
		lua_setglobal(L, "KeyBoardStateReload");
	}
	else
	{
		lua_pushstring(L, "FUCK");
		lua_setglobal(L, "KeyBoardStateReload");
	}

	if (sf::Mouse::isButtonPressed(sf::Mouse::Left))
	{
		lua_pushinteger(L, sf::Mouse::getPosition(*wndPtr).x);
		lua_setglobal(L, "MouseX");
		lua_pushinteger(L, sf::Mouse::getPosition(*wndPtr).y);
		lua_setglobal(L, "MouseY");
	}
	else
	{
		lua_pushinteger(L, -1);
		lua_setglobal(L, "MouseX");
		lua_pushinteger(L, -1);
		lua_setglobal(L, "MouseY");
	}

	lua_pushinteger(L, sf::Mouse::getPosition(*wndPtr).x);
	lua_setglobal(L, "MouseXX");
	lua_pushinteger(L, sf::Mouse::getPosition(*wndPtr).y);
	lua_setglobal(L, "MouseYY");

	lua_pushboolean(L, false);
	lua_setglobal(L, "SpawnEnemy");
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::O))
	{
		if (pressed == false)
		{
			//enemyList.push_back(new Enemy(L, 50, 50));
			lua_pushboolean(L, true);
			lua_setglobal(L, "SpawnEnemy");
			pressed = true;
		}
	}
	else
	{
		pressed = false;
	}

}

int GameHandler::random(lua_State * L)
{

	if (lua_isnumber(L, -1) && lua_isnumber(L, -2))
	{
		//p->AddingEnemy(lua_tonumber(L, -2), lua_tonumber(L, -1));
		int min = lua_tonumber(L, -2);
		int max = lua_tonumber(L, -1);
		lua_pop(L, 2);
		lua_pushnumber(L, min + (rand() % max));

		return 1;
	}
	else
		std::cout << "expect Rand(int min, int max)" << std::endl;
	return 0;
}



void GameHandler::draw(sf::RenderTarget & target, sf::RenderStates states) const
{
	if (DEBUG == 0) {
		if (player->getHealth() >= 0) {
			target.draw(*m_map);
			target.draw(*player);
			for (auto e : enemyList)
				target.draw(*e);

			bh.draw(target, states);


			//TextDraw
			target.draw(text);
			target.draw(hpText);
			target.draw(scoreText);
		}
		else
		{
			if (!keepAlive)
			{
				target.draw(deadText);
				target.draw(highScoreText);
			}
		}
	}
	else {
		
		target.draw(*m_map);
		target.draw(*player);
		for (auto e : enemyList)
			target.draw(*e);

		bh.draw(target, states);

		//TextDraw
		target.draw(text);
		target.draw(hpText);
		target.draw(scoreText);
	}
}

void GameHandler::_enemyFade()
{
	using namespace std::chrono_literals;
	/*for (size_t i = 0; i < deadEnemys.size(); i++)
	{
		if (deadEnemys.at(i).getFillColor().a > 10)
		{
			std::this_thread::sleep_for(50ms);
			sf::Color temp = deadEnemys.at(i).getFillColor();
			temp.a = temp.a - 1;
			deadEnemys.at(i).setFillColor(temp);
		}
		else
		{
			deadEnemys.erase(deadEnemys.begin() + i);
			break;
		}
	}*/
}
