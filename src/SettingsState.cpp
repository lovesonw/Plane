#include <Book/SettingsState.hpp>
#include <Book/Utility.hpp>
#include <Book/ResourceHolder.hpp>
#include <SFML/Graphics/RenderWindow.hpp>

SettingsState::SettingsState(StateStack& stack, Context context)
: State(stack, context)
, mGUIContainer()
{
	mBackgroundSprite.setTexture(context.textures->get(Textures::TitleScreen));

	// ������Ч��ť�����ǩ
	addButtonLabel(Player::MoveLeft,		300.f, "Left", context);
	addButtonLabel(Player::MoveRight,		350.f, "Right", context);
	addButtonLabel(Player::MoveUp,			400.f, "Up", context);
	addButtonLabel(Player::MoveDown,		450.f, "Down", context);
	addButtonLabel(Player::Fire,			500.f, "Fire", context);
	addButtonLabel(Player::LaunchMissile,	550.f, "Missile", context);
	updateLabels();
	auto backButton = std::make_shared<GUI::Button>(context);
	backButton->setPosition(80.f, 620.f);
	backButton->setText("Back");
	backButton->setCallback(std::bind(&SettingsState::requestStackPop, this));
	mGUIContainer.pack(backButton);
}

void SettingsState::draw()
{
	sf::RenderWindow& window = *getContext().window;
	window.draw(mBackgroundSprite);
	window.draw(mGUIContainer);
}

bool SettingsState::update(sf::Time)
{
	return true;
}

bool SettingsState::handleEvent(const sf::Event& event)
{
	bool isKeyBinding = false;

	// ������Ч�����ȴ�����
	for (std::size_t action = 0; action < Player::ActionCount; ++action)
	{
		if (mBindingButtons[action]->isActive())
		{
			isKeyBinding = true;
			if (event.type == sf::Event::KeyReleased)
			{
				getContext().player->assignKey(static_cast<Player::Action>(action), event.key.code);
				mBindingButtons[action]->deactivate();
			}
			break;
		}
	}

	// �����λ�ı䣬���ı�ǩ
	if (isKeyBinding)
		updateLabels();
	else
		mGUIContainer.handleEvent(event);
	return false;
}

void SettingsState::updateLabels()
{
	Player& player = *getContext().player;
	for (std::size_t i = 0; i < Player::ActionCount; ++i)
	{
		sf::Keyboard::Key key = player.getAssignedKey(static_cast<Player::Action>(i));
		mBindingLabels[i]->setText(toString(key));
	}
}

void SettingsState::addButtonLabel(Player::Action action, float y, const std::string& text, Context context)
{
	mBindingButtons[action] = std::make_shared<GUI::Button>(context);
	mBindingButtons[action]->setPosition(80.f, y);
	mBindingButtons[action]->setText(text);
	mBindingButtons[action]->setToggle(true);
	mBindingLabels[action] = std::make_shared<GUI::Label>("", *context.fonts);
	mBindingLabels[action]->setPosition(300.f, y + 15.f);
	mGUIContainer.pack(mBindingButtons[action]);
	mGUIContainer.pack(mBindingLabels[action]);
}
