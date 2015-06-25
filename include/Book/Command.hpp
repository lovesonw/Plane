#ifndef BOOK_COMMAND_HPP
#define BOOK_COMMAND_HPP

#include <Book/Category.hpp>
#include <SFML/System/Time.hpp>
#include <functional>
#include <cassert>

class SceneNode;

struct Command
{
	typedef std::function<void(SceneNode&, sf::Time)> Action;
								Command();
	Action						action;
	unsigned int				category;
};

template <typename GameObject, typename Function>
Command::Action derivedAction(Function fn)
{
	return [=] (SceneNode& node, sf::Time dt)
	{
		// 检查节点是否安全
		assert(dynamic_cast<GameObject*>(&node) != nullptr);
		fn(static_cast<GameObject&>(node), dt);
	};
}

#endif // BOOK_COMMAND_HPP
