#include <Book/World.hpp>
#include <Book/Projectile.hpp>
#include <Book/Pickup.hpp>
#include <Book/Foreach.hpp>
#include <Book/TextNode.hpp>
#include <Book/ParticleNode.hpp>
#include <Book/SoundNode.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

World::World(sf::RenderTarget& outputTarget, FontHolder& fonts, SoundPlayer& sounds)
: mTarget(outputTarget)
, mSceneTexture()
, mWorldView(outputTarget.getDefaultView())
, mTextures()
, mFonts(fonts)
, mSounds(sounds)
, mSceneGraph()
, mSceneLayers()
, mWorldBounds(0.f, 0.f, mWorldView.getSize().x, 5000.f)
, mSpawnPosition(mWorldView.getSize().x / 2.f, mWorldBounds.height - mWorldView.getSize().y / 2.f)
, mScrollSpeed(-50.f)
, mPlayerAircraft(nullptr)
, mEnemySpawnPoints()
, mActiveEnemies()
{
	mSceneTexture.create(mTarget.getSize().x, mTarget.getSize().y);
	loadTextures();
	buildScene();
	// 准备界面
	mWorldView.setCenter(mSpawnPosition);
}

void World::update(sf::Time dt)
{
	// 滚动地图，重设玩家速度
	mWorldView.move(0.f, mScrollSpeed * dt.asSeconds());
	mPlayerAircraft->setVelocity(0.f, 0.f);
	// 设置命令摧毁实体，引导导弹
	destroyEntitiesOutsideView();
	guideMissiles();
	// 转发命令到场景图, 调整速度（滚动速度和角度）
	while (!mCommandQueue.isEmpty())
		mSceneGraph.onCommand(mCommandQueue.pop(), dt);
	adaptPlayerVelocity();
	// 碰撞检测以及响应
	handleCollisions();
	// 移出所有被销毁的实体，产生新的敌人
	mSceneGraph.removeWrecks();
	spawnEnemies();
	// 上传每一步操作，判断位置是否超出边界
	mSceneGraph.update(dt, mCommandQueue);
	adaptPlayerPosition();
	updateSounds();
}

void World::draw()
{
	if (PostEffect::isSupported())
	{
		mSceneTexture.clear();
		mSceneTexture.setView(mWorldView);
		mSceneTexture.draw(mSceneGraph);
		mSceneTexture.display();
		mBloomEffect.apply(mSceneTexture, mTarget);
	}
	else
	{
		mTarget.setView(mWorldView);
		mTarget.draw(mSceneGraph);
	}
}

CommandQueue& World::getCommandQueue()
{
	return mCommandQueue;
}

bool World::hasAlivePlayer() const
{
	return !mPlayerAircraft->isMarkedForRemoval();
}

bool World::hasPlayerReachedEnd() const
{
	return !mWorldBounds.contains(mPlayerAircraft->getPosition());
}

void World::loadTextures()
{
	mTextures.load(Textures::Entities, "Media/Textures/Entities.png");
	mTextures.load(Textures::Jungle, "Media/Textures/Sea.png");
	mTextures.load(Textures::Explosion, "Media/Textures/Explosion.png");
	mTextures.load(Textures::Particle, "Media/Textures/Particle.png");
	mTextures.load(Textures::FinishLine, "Media/Textures/FinishLine.png");
}

void World::adaptPlayerPosition()
{
	// 保持玩家的位置不会超出边界
	sf::FloatRect viewBounds = getViewBounds();
	const float borderDistance = 40.f;
	sf::Vector2f position = mPlayerAircraft->getPosition();
	position.x = std::max(position.x, viewBounds.left + borderDistance);
	position.x = std::min(position.x, viewBounds.left + viewBounds.width - borderDistance);
	position.y = std::max(position.y, viewBounds.top + borderDistance);
	position.y = std::min(position.y, viewBounds.top + viewBounds.height - borderDistance);
	mPlayerAircraft->setPosition(position);
}

void World::adaptPlayerVelocity()
{
	sf::Vector2f velocity = mPlayerAircraft->getVelocity();
	// 如果对角移动的话，减小速度
	if (velocity.x != 0.f && velocity.y != 0.f)
		mPlayerAircraft->setVelocity(velocity / std::sqrt(2.f));
	// 添加滚动速度
	mPlayerAircraft->accelerate(0.f, mScrollSpeed);
}

bool matchesCategories(SceneNode::Pair& colliders, Category::Type type1, Category::Type type2)
{
	unsigned int category1 = colliders.first->getCategory();
	unsigned int category2 = colliders.second->getCategory();
	if (type1 & category1 && type2 & category2)
	{
		return true;
	}
	else if (type1 & category2 && type2 & category1)
	{
		std::swap(colliders.first, colliders.second);
		return true;
	}
	else
	{
		return false;
	}
}

void World::handleCollisions()
{
	std::set<SceneNode::Pair> collisionPairs;
	mSceneGraph.checkSceneCollision(mSceneGraph, collisionPairs);
	FOREACH(SceneNode::Pair pair, collisionPairs)
	{
		if (matchesCategories(pair, Category::PlayerAircraft, Category::EnemyAircraft))
		{
			auto& player = static_cast<Aircraft&>(*pair.first);
			auto& enemy = static_cast<Aircraft&>(*pair.second);
			// 碰撞：玩家伤害为敌机剩余伤害
			player.damage(enemy.getHitpoints());
			enemy.destroy();
		}
		else if (matchesCategories(pair, Category::PlayerAircraft, Category::Pickup))
		{
			auto& player = static_cast<Aircraft&>(*pair.first);
			auto& pickup = static_cast<Pickup&>(*pair.second);
			// 将补给品的效果加到玩家上，销毁补给品
			pickup.apply(player);
			pickup.destroy();
			player.playLocalSound(mCommandQueue, SoundEffect::CollectPickup);
		}
		else if (matchesCategories(pair, Category::EnemyAircraft, Category::AlliedProjectile)
			  || matchesCategories(pair, Category::PlayerAircraft, Category::EnemyProjectile))
		{
			auto& aircraft = static_cast<Aircraft&>(*pair.first);
			auto& projectile = static_cast<Projectile&>(*pair.second);
			// 敌对子弹碰撞，受到伤害，销毁子弹
			aircraft.damage(projectile.getDamage());
			projectile.destroy();
		}
	}
}

void World::updateSounds()
{
	// 设定声音的位置
	mSounds.setListenerPosition(mPlayerAircraft->getWorldPosition());
	// 删除未使用的声音
	mSounds.removeStoppedSounds();
}

void World::buildScene()
{
	// 初始化不同图层
	for (std::size_t i = 0; i < LayerCount; ++i)
	{
		Category::Type category = (i == LowerAir) ? Category::SceneAirLayer : Category::None;
		SceneNode::Ptr layer(new SceneNode(category));
		mSceneLayers[i] = layer.get();
		mSceneGraph.attachChild(std::move(layer));
	}

	// 准备平铺背景
	sf::Texture& jungleTexture = mTextures.get(Textures::Jungle);
	jungleTexture.setRepeated(true);
	float viewHeight = mWorldView.getSize().y;
	sf::IntRect textureRect(mWorldBounds);
	textureRect.height += static_cast<int>(viewHeight);
	// 添加背景
	std::unique_ptr<SpriteNode> jungleSprite(new SpriteNode(jungleTexture, textureRect));
	jungleSprite->setPosition(mWorldBounds.left, mWorldBounds.top - viewHeight);
	mSceneLayers[Background]->attachChild(std::move(jungleSprite));
	// 添加结束条幅
	sf::Texture& finishTexture = mTextures.get(Textures::FinishLine);
	std::unique_ptr<SpriteNode> finishSprite(new SpriteNode(finishTexture));
	finishSprite->setPosition(0.f, -76.f);
	mSceneLayers[Background]->attachChild(std::move(finishSprite));
	// 添加粒子节点
	std::unique_ptr<ParticleNode> smokeNode(new ParticleNode(Particle::Smoke, mTextures));
	mSceneLayers[LowerAir]->attachChild(std::move(smokeNode));
	// 添加推进效果
	std::unique_ptr<ParticleNode> propellantNode(new ParticleNode(Particle::Propellant, mTextures));
	mSceneLayers[LowerAir]->attachChild(std::move(propellantNode));
	// 添加音效
	std::unique_ptr<SoundNode> soundNode(new SoundNode(mSounds));
	mSceneGraph.attachChild(std::move(soundNode));
	// 添加玩家飞机
	std::unique_ptr<Aircraft> player(new Aircraft(Aircraft::Eagle, mTextures, mFonts));
	mPlayerAircraft = player.get();
	mPlayerAircraft->setPosition(mSpawnPosition);
	mSceneLayers[UpperAir]->attachChild(std::move(player));
	// 添加敌机
	addEnemies();
}

void World::addEnemies()
{
	//  添加敌机到指定位置
	addEnemy(Aircraft::Raptor,    0.f,  500.f);
	addEnemy(Aircraft::Raptor,    0.f, 1000.f);
	addEnemy(Aircraft::Raptor, +100.f, 1150.f);
	addEnemy(Aircraft::Raptor, -100.f, 1150.f);
	addEnemy(Aircraft::Avenger,  70.f, 1500.f);
	addEnemy(Aircraft::Avenger, -70.f, 1500.f);
	addEnemy(Aircraft::Avenger, -70.f, 1710.f);
	addEnemy(Aircraft::Avenger,  70.f, 1700.f);
	addEnemy(Aircraft::Avenger,  30.f, 1850.f);
	addEnemy(Aircraft::Raptor,  300.f, 2200.f);
	addEnemy(Aircraft::Raptor, -300.f, 2200.f);
	addEnemy(Aircraft::Raptor,    0.f, 2200.f);
	addEnemy(Aircraft::Raptor,    0.f, 2500.f);
	addEnemy(Aircraft::Avenger,-300.f, 2700.f);
	addEnemy(Aircraft::Avenger,-300.f, 2700.f);
	addEnemy(Aircraft::Raptor,    0.f, 3000.f);
	addEnemy(Aircraft::Raptor,  250.f, 3250.f);
	addEnemy(Aircraft::Raptor, -250.f, 3250.f);
	addEnemy(Aircraft::Avenger,   0.f, 3500.f);
	addEnemy(Aircraft::Avenger,   0.f, 3700.f);
	addEnemy(Aircraft::Raptor,    0.f, 3800.f);
	addEnemy(Aircraft::Avenger,   0.f, 4000.f);
	addEnemy(Aircraft::Avenger,-200.f, 4200.f);
	addEnemy(Aircraft::Raptor,  200.f, 4200.f);
	addEnemy(Aircraft::Raptor,    0.f, 4400.f);
	// 根据Y坐标排序所有敌机
	std::sort(mEnemySpawnPoints.begin(), mEnemySpawnPoints.end(), [] (SpawnPoint lhs, SpawnPoint rhs)
	{
		return lhs.y < rhs.y;
	});
}

void World::addEnemy(Aircraft::Type type, float relX, float relY)
{
	SpawnPoint spawn(type, mSpawnPosition.x + relX, mSpawnPosition.y - relY);
	mEnemySpawnPoints.push_back(spawn);
}

void World::spawnEnemies()
{
	// 产生敌机进入图像
	while (!mEnemySpawnPoints.empty()
		&& mEnemySpawnPoints.back().y > getBattlefieldBounds().top)
	{
		SpawnPoint spawn = mEnemySpawnPoints.back();
		std::unique_ptr<Aircraft> enemy(new Aircraft(spawn.type, mTextures, mFonts));
		enemy->setPosition(spawn.x, spawn.y);
		enemy->setRotation(180.f);
		mSceneLayers[UpperAir]->attachChild(std::move(enemy));
		mEnemySpawnPoints.pop_back();
	}
}

void World::destroyEntitiesOutsideView()
{
	Command command;
	command.category = Category::Projectile | Category::EnemyAircraft;
	command.action = derivedAction<Entity>([this] (Entity& e, sf::Time)
	{
		if (!getBattlefieldBounds().intersects(e.getBoundingRect()))
			e.remove();
	});

	mCommandQueue.push(command);
}

void World::guideMissiles()
{
	// 储存敌机
	Command enemyCollector;
	enemyCollector.category = Category::EnemyAircraft;
	enemyCollector.action = derivedAction<Aircraft>([this] (Aircraft& enemy, sf::Time)
	{
		if (!enemy.isDestroyed())
			mActiveEnemies.push_back(&enemy);
	});

	// 设置导弹攻击最近的目标
	Command missileGuider;
	missileGuider.category = Category::AlliedProjectile;
	missileGuider.action = derivedAction<Projectile>([this] (Projectile& missile, sf::Time)
	{
		// 忽略非指向性的导弹
		if (!missile.isGuided())
			return;
		float minDistance = std::numeric_limits<float>::max();
		Aircraft* closestEnemy = nullptr;
		// 找到最近的目标
		FOREACH(Aircraft* enemy, mActiveEnemies)
		{
			float enemyDistance = distance(missile, *enemy);

			if (enemyDistance < minDistance)
			{
				closestEnemy = enemy;
				minDistance = enemyDistance;
			}
		}
		if (closestEnemy)
			missile.guideTowards(closestEnemy->getWorldPosition());
	});
	mCommandQueue.push(enemyCollector);
	mCommandQueue.push(missileGuider);
	mActiveEnemies.clear();
}

sf::FloatRect World::getViewBounds() const
{
	return sf::FloatRect(mWorldView.getCenter() - mWorldView.getSize() / 2.f, mWorldView.getSize());
}

sf::FloatRect World::getBattlefieldBounds() const
{
	sf::FloatRect bounds = getViewBounds();
	bounds.top -= 100.f;
	bounds.height += 100.f;
	return bounds;
}
