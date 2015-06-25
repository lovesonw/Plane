#include <Book/Aircraft.hpp>
#include <Book/DataTables.hpp>
#include <Book/Utility.hpp>
#include <Book/Pickup.hpp>
#include <Book/CommandQueue.hpp>
#include <Book/SoundNode.hpp>
#include <Book/ResourceHolder.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <cmath>

using namespace std::placeholders;

namespace
{
	const std::vector<AircraftData> Table = initializeAircraftData();
}

Aircraft::Aircraft(Type type, const TextureHolder& textures, const FontHolder& fonts)
: Entity(Table[type].hitpoints)
, mType(type)
, mSprite(textures.get(Table[type].texture), Table[type].textureRect)
, mExplosion(textures.get(Textures::Explosion))
, mFireCommand()
, mMissileCommand()
, mFireCountdown(sf::Time::Zero)
, mIsFiring(false)
, mIsLaunchingMissile(false)
, mShowExplosion(true)
, mPlayedExplosionSound(false)
, mSpawnedPickup(false)
, mFireRateLevel(1)
, mSpreadLevel(1)
, mMissileAmmo(2)
, mDropPickupCommand()
, mTravelledDistance(0.f)
, mDirectionIndex(0)
, mHealthDisplay(nullptr)
, mMissileDisplay(nullptr)
{
	mExplosion.setFrameSize(sf::Vector2i(256, 256));
	mExplosion.setNumFrames(16);
	mExplosion.setDuration(sf::seconds(1));
	centerOrigin(mSprite);
	centerOrigin(mExplosion);
	mFireCommand.category = Category::SceneAirLayer;
	mFireCommand.action   = [this, &textures] (SceneNode& node, sf::Time)
	{
		createBullets(node, textures);
	};

	mMissileCommand.category = Category::SceneAirLayer;
	mMissileCommand.action   = [this, &textures] (SceneNode& node, sf::Time)
	{
		createProjectile(node, Projectile::Missile, 0.f, 0.5f, textures);
	};

	mDropPickupCommand.category = Category::SceneAirLayer;
	mDropPickupCommand.action   = [this, &textures] (SceneNode& node, sf::Time)
	{
		createPickup(node, textures);
	};
	std::unique_ptr<TextNode> healthDisplay(new TextNode(fonts, ""));
	mHealthDisplay = healthDisplay.get();
	attachChild(std::move(healthDisplay));
	if (getCategory() == Category::PlayerAircraft)
	{
		std::unique_ptr<TextNode> missileDisplay(new TextNode(fonts, ""));
		missileDisplay->setPosition(0, 70);
		mMissileDisplay = missileDisplay.get();
		attachChild(std::move(missileDisplay));
	}
	updateTexts();
}

void Aircraft::drawCurrent(sf::RenderTarget& target, sf::RenderStates states) const
{
	if (isDestroyed() && mShowExplosion)
		target.draw(mExplosion, states);
	else
		target.draw(mSprite, states);
}

void Aircraft::updateCurrent(sf::Time dt, CommandQueue& commands)
{
	// 上传数据及动画
	updateTexts();
	updateRollAnimation();
	// 实物毁灭，可能出现补给品
	if (isDestroyed())
	{
		checkPickupDrop(commands);
		mExplosion.update(dt);
		// 播放爆炸音效
		if (!mPlayedExplosionSound)
		{
			SoundEffect::ID soundEffect = (randomInt(2) == 0) ? SoundEffect::Explosion1 : SoundEffect::Explosion2;
			playLocalSound(commands, soundEffect);
			mPlayedExplosionSound = true;
		}
		return;
	}
	// 检查是否为子弹或者导弹被销毁
	checkProjectileLaunch(dt, commands);
	// 上传敌机移动动作
	updateMovementPattern(dt);
	Entity::updateCurrent(dt, commands);
}

unsigned int Aircraft::getCategory() const
{
	if (isAllied())
		return Category::PlayerAircraft;
	else
		return Category::EnemyAircraft;
}

sf::FloatRect Aircraft::getBoundingRect() const
{
	return getWorldTransform().transformRect(mSprite.getGlobalBounds());
}

bool Aircraft::isMarkedForRemoval() const
{
	return isDestroyed() && (mExplosion.isFinished() || !mShowExplosion);
}

void Aircraft::remove()
{
	Entity::remove();
	mShowExplosion = false;
}

bool Aircraft::isAllied() const
{
	return mType == Eagle;
}

float Aircraft::getMaxSpeed() const
{
	return Table[mType].speed;
}

void Aircraft::increaseFireRate()
{
    //增加子弹速率
	if (mFireRateLevel < 10)
		++mFireRateLevel;
}

void Aircraft::increaseSpread()
{
    //增加子弹数量，上限为3
	if (mSpreadLevel < 3)
		++mSpreadLevel;
}

void Aircraft::collectMissiles(unsigned int count)
{
    //增加导弹数量
	mMissileAmmo += count;
}

void Aircraft::fire()
{
	// 只有当开火间距不为0时，可以开火
	if (Table[mType].fireInterval != sf::Time::Zero)
		mIsFiring = true;
}

void Aircraft::launchMissile()
{
    // 导弹数大于0时可以使用导弹
	if (mMissileAmmo > 0)
	{
		mIsLaunchingMissile = true;
		--mMissileAmmo;
	}
}

void Aircraft::playLocalSound(CommandQueue& commands, SoundEffect::ID effect)
{
	sf::Vector2f worldPosition = getWorldPosition();

	Command command;
	command.category = Category::SoundEffect;
	command.action = derivedAction<SoundNode>(
		[effect, worldPosition] (SoundNode& node, sf::Time)
		{
			node.playSound(effect, worldPosition);
		});

	commands.push(command);
}

void Aircraft::updateMovementPattern(sf::Time dt)
{
	// 敌机运动
	const std::vector<Direction>& directions = Table[mType].directions;
	if (!directions.empty())
	{
		// 当移动一定距离后改变方向
		if (mTravelledDistance > directions[mDirectionIndex].distance)
		{
			mDirectionIndex = (mDirectionIndex + 1) % directions.size();
			mTravelledDistance = 0.f;
		}
		// 计算速度
		float radians = toRadian(directions[mDirectionIndex].angle + 90.f);
		float vx = getMaxSpeed() * std::cos(radians);
		float vy = getMaxSpeed() * std::sin(radians);
		setVelocity(vx, vy);
		mTravelledDistance += getMaxSpeed() * dt.asSeconds();
	}
}

void Aircraft::checkPickupDrop(CommandQueue& commands)
{
	if (!isAllied() && randomInt(3) == 0 && !mSpawnedPickup)
		commands.push(mDropPickupCommand);
	mSpawnedPickup = true;
}

void Aircraft::checkProjectileLaunch(sf::Time dt, CommandQueue& commands)
{
	// 敌机尝试一直开火
	if (!isAllied())
		fire();

	// 检查自动开枪，只允许在间隔范围内
	if (mIsFiring && mFireCountdown <= sf::Time::Zero)
	{
		// 间距到了需要创造一个新的子弹
		commands.push(mFireCommand);
		playLocalSound(commands, isAllied() ? SoundEffect::AlliedGunfire : SoundEffect::EnemyGunfire);
		mFireCountdown += Table[mType].fireInterval / (mFireRateLevel + 1.f);
		mIsFiring = false;
	}
	else if (mFireCountdown > sf::Time::Zero)
	{
		// 间距未到，减少开火倒计时
		mFireCountdown -= dt;
		mIsFiring = false;
	}
	// 检查导弹发射
	if (mIsLaunchingMissile)
	{
		commands.push(mMissileCommand);
		playLocalSound(commands, SoundEffect::LaunchMissile);
		mIsLaunchingMissile = false;
	}
}

void Aircraft::createBullets(SceneNode& node, const TextureHolder& textures) const
{
	Projectile::Type type = isAllied() ? Projectile::AlliedBullet : Projectile::EnemyBullet;
	switch (mSpreadLevel)
	{
		case 1:
			createProjectile(node, type, 0.0f, 0.5f, textures);
			break;
		case 2:
			createProjectile(node, type, -0.33f, 0.33f, textures);
			createProjectile(node, type, +0.33f, 0.33f, textures);
			break;
		case 3:
			createProjectile(node, type, -0.5f, 0.33f, textures);
			createProjectile(node, type,  0.0f, 0.5f, textures);
			createProjectile(node, type, +0.5f, 0.33f, textures);
			break;
	}
}

void Aircraft::createProjectile(SceneNode& node, Projectile::Type type, float xOffset, float yOffset, const TextureHolder& textures) const
{
	//子弹移动
	std::unique_ptr<Projectile> projectile(new Projectile(type, textures));
	sf::Vector2f offset(xOffset * mSprite.getGlobalBounds().width, yOffset * mSprite.getGlobalBounds().height);
	sf::Vector2f velocity(0, projectile->getMaxSpeed());
	float sign = isAllied() ? -1.f : +1.f;
	projectile->setPosition(getWorldPosition() + offset * sign);
	projectile->setVelocity(velocity * sign);
	node.attachChild(std::move(projectile));
}

void Aircraft::createPickup(SceneNode& node, const TextureHolder& textures) const
{
	auto type = static_cast<Pickup::Type>(randomInt(Pickup::TypeCount));
	std::unique_ptr<Pickup> pickup(new Pickup(type, textures));
	pickup->setPosition(getWorldPosition());
	pickup->setVelocity(0.f, 1.f);
	node.attachChild(std::move(pickup));
}

void Aircraft::updateTexts()
{
	// 显示生命值
	if (isDestroyed())
		mHealthDisplay->setString("");
	else
		mHealthDisplay->setString(toString(getHitpoints()) + " HP");
	mHealthDisplay->setPosition(0.f, 50.f);
	mHealthDisplay->setRotation(-getRotation());
	// 如果有导弹，显示其数量
	if (mMissileDisplay)
	{
		if (mMissileAmmo == 0 || isDestroyed())
			mMissileDisplay->setString("");
		else
			mMissileDisplay->setString("M: " + toString(mMissileAmmo));
	}
}

void Aircraft::updateRollAnimation()
{
	if (Table[mType].hasRollAnimation)
	{
		sf::IntRect textureRect = Table[mType].textureRect;
		// 向左滚动，素材矩形移动一次
		if (getVelocity().x < 0.f)
			textureRect.left += textureRect.width;
		// 向右滚动: 素材矩形移动两次
		else if (getVelocity().x > 0.f)
			textureRect.left += 2 * textureRect.width;
		mSprite.setTextureRect(textureRect);
	}
}
