#include "Core/Physics/Physics2D.h"
#include "Core/Physics/Contacts2D.h"
#include "Core/Math/MathUtils.h"
#include "Core/World/Transform.h"
#include "Core/World/World.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace Flex
{
	FLEX_REGISTER_COMPONENT(RigidBody2D)
	FLEX_REGISTER_COMPONENT(BoxCollider2D)
	FLEX_REGISTER_COMPONENT(CircleCollider2D)

	const char* ToString(BodyType type)
	{
		switch (type) {
		case BodyType::Static:
			return "Static";
		case BodyType::Kinematic:
			return "Kinematic";
		case BodyType::Dynamic:
			return "Dynamic";
		}
		return "Dynamic";
	}

	bool TryParseBodyType(const std::string& text, BodyType& outType)
	{
		for (BodyType type : { BodyType::Static, BodyType::Kinematic, BodyType::Dynamic }) {
			if (text == ToString(type)) {
				outType = type;
				return true;
			}
		}
		return false;
	}

	using namespace Physics;

	namespace
	{
		constexpr float Baumgarte = 0.2f;      // fraction of an overlap fixed per position iteration
		constexpr float MaxCorrection = 8.0f;  // units per position iteration
		constexpr float MinMass = 0.0001f;
		constexpr float MinCellSize = 64.0f;
		constexpr int32_t MaxCellsPerAxis = 256;
		constexpr size_t MaxStaticsWithoutGrid = 16; // fewer static colliders are simply tested one by one

		// Bodies in a uniform grid over their bounds, stored per cell (counted, then filled).
		class UniformGrid
		{
		public:
			void Build(const std::vector<SolverBody>& bodies, const std::vector<uint32_t>& items)
			{
				Items.clear();
				Width = Height = 0;
				if (items.empty()) {
					return;
				}
				Vector2 min = bodies[items[0]].Min;
				Vector2 max = bodies[items[0]].Max;
				for (uint32_t item : items) {
					min = { std::min(min.x, bodies[item].Min.x), std::min(min.y, bodies[item].Min.y) };
					max = { std::max(max.x, bodies[item].Max.x), std::max(max.y, bodies[item].Max.y) };
				}
				Origin = min;
				CellSize = std::max({ MinCellSize, (max.x - min.x) / MaxCellsPerAxis, (max.y - min.y) / MaxCellsPerAxis });
				InvCellSize = 1.0f / CellSize;
				Width = std::clamp(static_cast<int32_t>((max.x - min.x) / CellSize) + 1, 1, MaxCellsPerAxis);
				Height = std::clamp(static_cast<int32_t>((max.y - min.y) / CellSize) + 1, 1, MaxCellsPerAxis);

				CellStart.assign(static_cast<size_t>(Width) * Height + 1, 0);
				for (uint32_t item : items) {
					ForEachCell(bodies[item], [&](int32_t cell) { ++CellStart[cell + 1]; });
				}
				for (size_t i = 1; i < CellStart.size(); ++i) {
					CellStart[i] += CellStart[i - 1];
				}
				Items.resize(CellStart.back());
				Fill.assign(CellStart.begin(), CellStart.end() - 1);
				for (uint32_t item : items) {
					ForEachCell(bodies[item], [&](int32_t cell) { Items[Fill[cell]++] = item; });
				}
			}

			// Calls func(item) once for every item whose cell overlaps body's bounds. An item that
			// shares several cells is reported only in the cell where the two bounds' overlap starts.
			template<typename Func>
			void Query(const std::vector<SolverBody>& bodies, const SolverBody& body, Func&& func) const
			{
				if (Width == 0) {
					return;
				}
				int32_t x0, y0, x1, y1;
				if (!GetCellRange(body.Min, body.Max, x0, y0, x1, y1)) {
					return;
				}
				for (int32_t y = y0; y <= y1; ++y) {
					for (int32_t x = x0; x <= x1; ++x) {
						const int32_t cell = y * Width + x;
						for (uint32_t i = CellStart[cell]; i < CellStart[cell + 1]; ++i) {
							const SolverBody& other = bodies[Items[i]];
							if (other.Min.x > body.Max.x || other.Max.x < body.Min.x || other.Min.y > body.Max.y || other.Max.y < body.Min.y) {
								continue;
							}
							if (ToCellX(std::max(body.Min.x, other.Min.x)) != x || ToCellY(std::max(body.Min.y, other.Min.y)) != y) {
								continue;
							}
							func(Items[i]);
						}
					}
				}
			}

			// Whether any item's bounds overlap body's bounds.
			bool OverlapsAny(const std::vector<SolverBody>& bodies, const Vector2& min, const Vector2& max) const
			{
				int32_t x0, y0, x1, y1;
				if (Width == 0 || !GetCellRange(min, max, x0, y0, x1, y1)) {
					return false;
				}
				for (int32_t y = y0; y <= y1; ++y) {
					for (int32_t x = x0; x <= x1; ++x) {
						const int32_t cell = y * Width + x;
						for (uint32_t i = CellStart[cell]; i < CellStart[cell + 1]; ++i) {
							const SolverBody& other = bodies[Items[i]];
							if (other.Min.x <= max.x && other.Max.x >= min.x && other.Min.y <= max.y && other.Max.y >= min.y) {
								return true;
							}
						}
					}
				}
				return false;
			}

		private:
			// Truncating is enough: everything left of/above the origin clamps to cell 0 anyway.
			int32_t ToCellX(float value) const { return std::clamp(static_cast<int32_t>((value - Origin.x) * InvCellSize), 0, Width - 1); }
			int32_t ToCellY(float value) const { return std::clamp(static_cast<int32_t>((value - Origin.y) * InvCellSize), 0, Height - 1); }

			bool GetCellRange(const Vector2& min, const Vector2& max, int32_t& x0, int32_t& y0, int32_t& x1, int32_t& y1) const
			{
				const float right = Origin.x + Width * CellSize;
				const float bottom = Origin.y + Height * CellSize;
				if (max.x < Origin.x || max.y < Origin.y || min.x > right || min.y > bottom) {
					return false;
				}
				x0 = ToCellX(min.x);
				y0 = ToCellY(min.y);
				x1 = ToCellX(max.x);
				y1 = ToCellY(max.y);
				return true;
			}

			template<typename Func>
			void ForEachCell(const SolverBody& body, Func&& func) const
			{
				int32_t x0, y0, x1, y1;
				GetCellRange(body.Min, body.Max, x0, y0, x1, y1);
				for (int32_t y = y0; y <= y1; ++y) {
					for (int32_t x = x0; x <= x1; ++x) {
						func(y * Width + x);
					}
				}
			}

			Vector2 Origin;
			float CellSize = MinCellSize;
			float InvCellSize = 1.0f / MinCellSize;
			int32_t Width = 0;
			int32_t Height = 0;
			std::vector<uint32_t> CellStart;
			std::vector<uint32_t> Fill;
			std::vector<uint32_t> Items;
		};

		// The physics components of one archetype (nullptr = it doesn't have that one).
		struct BodyTable
		{
			uint32_t Count = 0;
			const Entity* Entities = nullptr;
			RigidBody2D* Bodies = nullptr;
			const BoxCollider2D* Boxes = nullptr;
			const CircleCollider2D* Circles = nullptr;
			Transform* Transforms = nullptr;
			const WorldTransform* WorldTransforms = nullptr;
			const Archetype* Source = nullptr;
			bool bHasStatics = false;
		};

		// Pair key -> index, open addressing (rebuilt every step, so no erasing).
		class PairTable
		{
		public:
			void Reset(size_t count)
			{
				size_t size = 16;
				Bits = 4;
				while (size < count * 2) {
					size *= 2;
					++Bits;
				}
				Keys.assign(size, EmptyKey);
				Values.resize(size);
			}

			void Insert(uint64_t key, uint32_t value)
			{
				size_t slot = Hash(key);
				while (Keys[slot] != EmptyKey) {
					slot = (slot + 1) & (Keys.size() - 1);
				}
				Keys[slot] = key;
				Values[slot] = value;
			}

			bool Find(uint64_t key, uint32_t& outValue) const
			{
				for (size_t slot = Hash(key); Keys[slot] != EmptyKey; slot = (slot + 1) & (Keys.size() - 1)) {
					if (Keys[slot] == key) {
						outValue = Values[slot];
						return true;
					}
				}
				return false;
			}

		private:
			static constexpr uint64_t EmptyKey = ~0ull;
			size_t Hash(uint64_t key) const { return static_cast<size_t>((key * 0x9E3779B97F4A7C15ull) >> (64 - Bits)); }

			std::vector<uint64_t> Keys;
			std::vector<uint32_t> Values;
			uint32_t Bits = 4;
		};

		uint64_t MakePairKey(Entity a, Entity b)
		{
			const uint32_t low = std::min(a.Index, b.Index);
			const uint32_t high = std::max(a.Index, b.Index);
			return (static_cast<uint64_t>(high) << 32) | low;
		}

		bool ShouldCollide(const SolverBody& a, const SolverBody& b)
		{
			if ((a.Layer & b.Mask) == 0 || (b.Layer & a.Mask) == 0) {
				return false;
			}
			// Something has to move, and something has to be pushable.
			const bool bActive = a.IsAwakeDynamic() || b.IsAwakeDynamic() || a.IsMovingKinematic() || b.IsMovingKinematic();
			const bool bPushable = a.Type == BodyType::Dynamic || b.Type == BodyType::Dynamic;
			return bActive && bPushable;
		}

		// Bounding box, grown by the contact margin.
		void ComputeBounds(SolverBody& body, float margin)
		{
			if (body.bIsCircle) {
				body.Min = body.Center - Vector2{ body.Radius + margin, body.Radius + margin };
				body.Max = body.Center + Vector2{ body.Radius + margin, body.Radius + margin };
				return;
			}
			const float extentX = std::abs(body.Rot.Cos) * body.HalfSize.x + std::abs(body.Rot.Sin) * body.HalfSize.y;
			const float extentY = std::abs(body.Rot.Sin) * body.HalfSize.x + std::abs(body.Rot.Cos) * body.HalfSize.y;
			body.Min = body.Center - Vector2{ extentX + margin, extentY + margin };
			body.Max = body.Center + Vector2{ extentX + margin, extentY + margin };
		}

		int32_t FindRoot(std::vector<int32_t>& parents, int32_t index)
		{
			while (parents[index] != index) {
				parents[index] = parents[parents[index]];
				index = parents[index];
			}
			return index;
		}
	}

	struct PhysicsWorld2D::State
	{
		std::vector<SolverBody> Bodies;
		std::vector<uint32_t> StaticItems;
		std::vector<uint32_t> MovingItems; // dynamic + kinematic bodies that can hit each other
		UniformGrid StaticGrid;
		UniformGrid MovingGrid;

		std::vector<Manifold> Manifolds;
		std::vector<uint64_t> ManifoldKeys;
		std::vector<Manifold> PreviousManifolds;
		std::vector<Entity> PreviousEntities; // A and B of each previous manifold
		PairTable PreviousLookup;

		std::vector<BodyTable> Tables;
		std::vector<std::pair<float, uint32_t>> Order; // manifold sort scratch
		std::vector<Manifold> SortedManifolds;
		std::vector<uint64_t> SortedKeys;
		std::vector<const Archetype*> StaticTables; // tables with Static bodies last step
		uint32_t KnownMovingLayers = 0;              // every layer/mask moving bodies used so far
		uint32_t KnownMovingMasks = 0;
		bool bLayersKnown = false;
		std::vector<int32_t> IslandParents;
		std::vector<float> IslandMinSleep;

		void Gather(World& world, const PhysicsSettings& settings, float deltaTime, PhysicsStats& stats);
		void FindContacts(float linearSlop);
		void OrderManifolds(const Vector2& gravity);
		void WarmStartFromPrevious(float linearSlop);
		void WakeTouchedBodies();
		void SolveVelocities(const PhysicsSettings& settings, float deltaTime, bool bFirstSubstep);
		void SolvePositions(const PhysicsSettings& settings);
		void UpdateSleep(const PhysicsSettings& settings, float deltaTime, PhysicsStats& stats);
		void KeepManifolds();
	};

	PhysicsWorld2D::PhysicsWorld2D() : Data{ std::make_unique<State>() } {}
	PhysicsWorld2D::~PhysicsWorld2D() = default;
	PhysicsWorld2D::PhysicsWorld2D(PhysicsWorld2D&&) noexcept = default;
	PhysicsWorld2D& PhysicsWorld2D::operator=(PhysicsWorld2D&&) noexcept = default;

	void PhysicsWorld2D::Reset()
	{
		Data = std::make_unique<State>();
	}

	namespace
	{
		// Applies gravity and drag (awake dynamic bodies).
		void IntegrateVelocity(SolverBody& body, const PhysicsSettings& settings, float deltaTime)
		{
			const RigidBody2D& rigidBody = *body.Body;
			body.Velocity += settings.Gravity * (rigidBody.GravityScale * deltaTime);
			if (rigidBody.LinearDrag > 0.0f) {
				body.Velocity *= std::max(0.0f, 1.0f - rigidBody.LinearDrag * deltaTime);
			}
			if (rigidBody.AngularDrag > 0.0f) {
				body.AngularVelocity *= std::max(0.0f, 1.0f - rigidBody.AngularDrag * deltaTime);
			}
		}

		void IntegratePosition(SolverBody& body, float deltaTime)
		{
			body.Center += body.Velocity * deltaTime;
			if (body.AngularVelocity != 0.0f) {
				body.Angle += body.AngularVelocity * deltaTime;
				body.Rot = Rotation2::FromAngle(body.Angle);
			}
		}

		// Counts how long an awake dynamic body has been lying still.
		void UpdateSleepTimer(RigidBody2D& rigidBody, const Vector2& velocity, float angularVelocity, const PhysicsSettings& settings, float deltaTime)
		{
			if (!settings.bAllowSleep || !rigidBody.bCanSleep || velocity.LengthSquared() > settings.SleepLinearSpeed * settings.SleepLinearSpeed
				|| std::abs(angularVelocity) > settings.SleepAngularSpeed) {
				rigidBody.SleepTimer = 0.0f;
			} else {
				rigidBody.SleepTimer += deltaTime;
			}
		}

		// Writes position, rotation, velocities and the sleep state back to the components.
		void WriteBody(const SolverBody& body)
		{
			const bool bOffset = body.LocalCenter.x != 0.0f || body.LocalCenter.y != 0.0f;
			body.TransformData->Position = body.Center - (bOffset ? Rotation2::FromAngle(body.Angle).Rotate(body.LocalCenter) : Vector2{});
			if (body.Angle != body.TransformData->Rotation) {
				body.TransformData->Rotation = (body.Angle > Math::Pi || body.Angle <= -Math::Pi) ? Math::WrapAngle(body.Angle) : body.Angle;
			}
			RigidBody2D& rigidBody = *body.Body;
			rigidBody.Velocity = body.Velocity;
			if (!rigidBody.bFixedRotation) {
				rigidBody.AngularVelocity = body.AngularVelocity;
			}
			if (body.Type == BodyType::Dynamic) {
				rigidBody.bIsSleeping = body.bSleeping;
			}
		}

		// Reads one row (shape, pose, mass). False when the row can't be simulated.
		bool ReadBody(const BodyTable& table, uint32_t row, float margin, SolverBody& body)
		{
			RigidBody2D* rigidBody = table.Bodies ? &table.Bodies[row] : nullptr;
			const BoxCollider2D* box = table.Boxes ? &table.Boxes[row] : nullptr;
			const CircleCollider2D* circle = table.Circles ? &table.Circles[row] : nullptr;
			const BodyType type = rigidBody ? rigidBody->Type : BodyType::Static;
			body.Owner = table.Entities[row];
			body.Body = rigidBody;
			body.Type = type;

			if (type == BodyType::Static) {
				// Static colliders may be children: they use the world transform.
				if (!table.WorldTransforms || (!box && !circle)) {
					return false;
				}
				const ColliderShape2D shape = box ? ColliderShape2D::FromBox(table.WorldTransforms[row].Matrix, *box)
					: ColliderShape2D::FromCircle(table.WorldTransforms[row].Matrix, *circle);
				body.bHasShape = true;
				body.bIsCircle = shape.bIsCircle;
				body.Center = shape.Center;
				body.Angle = shape.Rotation;
				body.Rot = Rotation2::FromAngle(body.Angle);
				body.HalfSize = shape.HalfSize;
				body.Radius = shape.Radius;
			} else {
				if (!table.Transforms) {
					return false;
				}
				Transform& transform = table.Transforms[row];
				body.TransformData = &transform;
				body.Angle = transform.Rotation;
				const Vector2 scale{ std::abs(transform.Scale.x), std::abs(transform.Scale.y) };
				if (box) {
					body.bHasShape = true;
					body.LocalCenter = box->Offset * transform.Scale;
					body.HalfSize = { std::abs(box->Size.x) * scale.x * 0.5f, std::abs(box->Size.y) * scale.y * 0.5f };
				} else if (circle) {
					body.bHasShape = true;
					body.bIsCircle = true;
					body.LocalCenter = circle->Offset * transform.Scale;
					body.Radius = std::abs(circle->Radius) * std::max(scale.x, scale.y);
				}
				// A circle around its origin never needs its rotation (saves the trigonometry).
				const bool bOffset = body.LocalCenter.x != 0.0f || body.LocalCenter.y != 0.0f;
				if (!body.bIsCircle || bOffset) {
					body.Rot = Rotation2::FromAngle(body.Angle);
				}
				body.Center = transform.Position + (bOffset ? body.Rot.Rotate(body.LocalCenter) : Vector2{});
			}

			if (box) {
				body.Layer = box->Layer;
				body.Mask = box->Mask;
			} else if (circle) {
				body.Layer = circle->Layer;
				body.Mask = circle->Mask;
			}

			if (rigidBody) {
				body.Friction = std::max(rigidBody->Friction, 0.0f);
				body.Restitution = std::max(rigidBody->Restitution, 0.0f);
				if (type != BodyType::Static) {
					body.Velocity = rigidBody->Velocity;
					body.AngularVelocity = rigidBody->bFixedRotation ? 0.0f : rigidBody->AngularVelocity;
				}
				if (type == BodyType::Dynamic) {
					// Game code setting a velocity wakes a sleeping body.
					if (rigidBody->bIsSleeping && (body.Velocity.LengthSquared() > 0.0f || body.AngularVelocity != 0.0f)) {
						rigidBody->bIsSleeping = false;
						rigidBody->SleepTimer = 0.0f;
					}
					body.bSleeping = rigidBody->bIsSleeping;
					const float mass = std::max(rigidBody->Mass, MinMass);
					body.InvMass = 1.0f / mass;
					float inertia = 0.0f;
					if (body.bHasShape) {
						inertia = body.bIsCircle ? 0.5f * mass * body.Radius * body.Radius
							: mass * (body.HalfSize.x * body.HalfSize.x + body.HalfSize.y * body.HalfSize.y) / 3.0f; // m(w^2 + h^2) / 12
					}
					body.InvInertia = (!rigidBody->bFixedRotation && inertia > 0.0f) ? 1.0f / inertia : 0.0f;
				}
			}
			if (body.bHasShape) {
				ComputeBounds(body, margin);
			}
			return true;
		}
	}

	void PhysicsWorld2D::State::Gather(World& world, const PhysicsSettings& settings, float deltaTime, PhysicsStats& stats)
	{
		Bodies.clear();
		StaticItems.clear();
		MovingItems.clear();
		Tables.clear();
		const float margin = std::max(settings.LinearSlop, 0.0f); // contact margin
		const ComponentId transformId = GetComponentId<Transform>();
		const ComponentId worldTransformId = GetComponentId<WorldTransform>();
		const ComponentId bodyId = GetComponentId<RigidBody2D>();
		const ComponentId boxId = GetComponentId<BoxCollider2D>();
		const ComponentId circleId = GetComponentId<CircleCollider2D>();

		for (const std::unique_ptr<Archetype>& archetype : world.GetArchetypes()) {
			const int32_t bodyColumn = archetype->FindColumn(bodyId);
			const int32_t boxColumn = archetype->FindColumn(boxId);
			const int32_t circleColumn = archetype->FindColumn(circleId);
			if (archetype->GetCount() == 0 || (bodyColumn < 0 && boxColumn < 0 && circleColumn < 0)) {
				continue;
			}
			const int32_t transformColumn = archetype->FindColumn(transformId);
			const int32_t worldTransformColumn = archetype->FindColumn(worldTransformId);
			BodyTable& table = Tables.emplace_back();
			table.Source = archetype.get();
			table.Count = archetype->GetCount();
			table.Entities = archetype->GetEntities();
			table.Bodies = bodyColumn >= 0 ? static_cast<RigidBody2D*>(archetype->GetColumnData(bodyColumn)) : nullptr;
			table.Boxes = boxColumn >= 0 ? static_cast<const BoxCollider2D*>(archetype->GetColumnData(boxColumn)) : nullptr;
			table.Circles = circleColumn >= 0 ? static_cast<const CircleCollider2D*>(archetype->GetColumnData(circleColumn)) : nullptr;
			table.Transforms = transformColumn >= 0 ? static_cast<Transform*>(archetype->GetColumnData(transformColumn)) : nullptr;
			table.WorldTransforms = worldTransformColumn >= 0 ? static_cast<const WorldTransform*>(archetype->GetColumnData(worldTransformColumn)) : nullptr;
		}

		// 1. Static colliders: every table without bodies, and the tables that held Static
		// bodies last step. The first step scans every table, also for the layers moving bodies use.
		const bool bFullScan = !bLayersKnown;
		bLayersKnown = true;
		auto addStatic = [&](const BodyTable& table, uint32_t row) {
			SolverBody body;
			if (ReadBody(table, row, margin, body)) {
				StaticItems.push_back(static_cast<uint32_t>(Bodies.size()));
				Bodies.push_back(body);
			}
		};
		auto scanTable = [&](BodyTable& table, const auto* colliders) {
			for (uint32_t row = 0; row < table.Count; ++row) {
				if (table.Bodies[row].Type == BodyType::Static) {
					addStatic(table, row);
					table.bHasStatics = true;
				} else if (bFullScan) {
					KnownMovingLayers |= colliders[row].Layer;
					KnownMovingMasks |= colliders[row].Mask;
				}
			}
		};
		for (BodyTable& table : Tables) {
			if (!table.Bodies) {
				for (uint32_t row = 0; row < table.Count; ++row) {
					addStatic(table, row);
				}
			} else if (bFullScan || std::find(StaticTables.begin(), StaticTables.end(), table.Source) != StaticTables.end()) {
				if (table.Boxes) {
					scanTable(table, table.Boxes);
				} else if (table.Circles) {
					scanTable(table, table.Circles);
				}
			}
		}
		stats.StaticColliders = static_cast<uint32_t>(StaticItems.size());
		StaticGrid.Build(Bodies, StaticItems); // the statics are Bodies[0, StaticItems.size())

		// 2. Moving bodies. Only those that can touch something become solver bodies (bodies
		// whose layers can meet another moving body's, or that are near a static collider);
		// the rest (e.g. a swarm flying through open space) are moved right here.
		uint32_t movingLayers = 0;
		uint32_t movingMasks = 0;
		for (BodyTable& table : Tables) {
			if (!table.Bodies) {
				continue;
			}
			for (uint32_t row = 0; row < table.Count; ++row) {
				RigidBody2D& rigidBody = table.Bodies[row];
				if (rigidBody.Type == BodyType::Static) {
					// A body that became Static in a table that had none is a static collider from the next step.
					table.bHasStatics = true;
					continue;
				}
				if (!table.Transforms) {
					continue;
				}
				const bool bDynamic = rigidBody.Type == BodyType::Dynamic;
				if (bDynamic) {
					++stats.DynamicBodies;
				}
				Transform& transform = table.Transforms[row];

				// Quick look (no trigonometry): a circle around the collider's centre that holds it.
				const BoxCollider2D* box = table.Boxes ? &table.Boxes[row] : nullptr;
				const CircleCollider2D* circle = table.Circles ? &table.Circles[row] : nullptr;
				bool bMeetsMoving = false;
				bool bNearStatic = false;
				if (box || circle) {
					const uint32_t layer = box ? box->Layer : circle->Layer;
					const uint32_t mask = box ? box->Mask : circle->Mask;
					// Layers decide from what moving bodies used up to last step (new layers count from the next step).
					movingLayers |= layer;
					movingMasks |= mask;
					bMeetsMoving = (layer & KnownMovingMasks) != 0 && (mask & KnownMovingLayers) != 0;
					const bool bAwake = !bDynamic || !rigidBody.bIsSleeping || rigidBody.Velocity.LengthSquared() > 0.0f || rigidBody.AngularVelocity != 0.0f;
					if (!bMeetsMoving && bDynamic && bAwake && !StaticItems.empty()) {
						const Vector2 scale{ std::abs(transform.Scale.x), std::abs(transform.Scale.y) };
						const Vector2 offset = (box ? box->Offset : circle->Offset) * transform.Scale;
						const float radius = box ? (std::abs(box->Size.x) * scale.x + std::abs(box->Size.y) * scale.y) * 0.5f
							: std::abs(circle->Radius) * std::max(scale.x, scale.y);
						const float reach = radius + std::abs(offset.x) + std::abs(offset.y) + margin;
						const Vector2 min = transform.Position - Vector2{ reach, reach };
						const Vector2 max = transform.Position + Vector2{ reach, reach };
						if (StaticItems.size() <= MaxStaticsWithoutGrid) {
							for (const SolverBody* other = Bodies.data(), *end = other + StaticItems.size(); other != end && !bNearStatic; ++other) {
								bNearStatic = other->Min.x <= max.x && other->Max.x >= min.x && other->Min.y <= max.y && other->Max.y >= min.y;
							}
						} else {
							bNearStatic = StaticGrid.OverlapsAny(Bodies, min, max);
						}
					}
				}
				if (bMeetsMoving || bNearStatic) {
					SolverBody body;
					if (ReadBody(table, row, margin, body)) {
						if (bMeetsMoving) {
							MovingItems.push_back(static_cast<uint32_t>(Bodies.size()));
						}
						Bodies.push_back(body);
					}
					continue;
				}

				// Touches nothing: move it right here.
				if (bDynamic) {
					if (rigidBody.bIsSleeping) {
						if (rigidBody.Velocity.LengthSquared() == 0.0f && rigidBody.AngularVelocity == 0.0f) {
							++stats.SleepingBodies;
							continue;
						}
						rigidBody.bIsSleeping = false; // game code gave it a velocity
						rigidBody.SleepTimer = 0.0f;
					}
					rigidBody.Velocity += settings.Gravity * (rigidBody.GravityScale * deltaTime);
					if (rigidBody.LinearDrag > 0.0f) {
						rigidBody.Velocity *= std::max(0.0f, 1.0f - rigidBody.LinearDrag * deltaTime);
					}
					if (rigidBody.AngularDrag > 0.0f) {
						rigidBody.AngularVelocity *= std::max(0.0f, 1.0f - rigidBody.AngularDrag * deltaTime);
					}
				}
				const float spin = rigidBody.bFixedRotation ? 0.0f : rigidBody.AngularVelocity;
				transform.Position += rigidBody.Velocity * deltaTime;
				if (spin != 0.0f) {
					// Turning around the collider's centre moves the origin if the collider is offset.
					const Vector2 offset = box ? box->Offset * transform.Scale : circle ? circle->Offset * transform.Scale : Vector2{};
					const float angle = transform.Rotation + spin * deltaTime;
					if (offset.x != 0.0f || offset.y != 0.0f) {
						transform.Position += offset.Rotated(transform.Rotation) - offset.Rotated(angle);
					}
					transform.Rotation = (angle > Math::Pi || angle <= -Math::Pi) ? Math::WrapAngle(angle) : angle;
				}
				if (bDynamic) {
					UpdateSleepTimer(rigidBody, rigidBody.Velocity, spin, settings, deltaTime);
					if (settings.bAllowSleep && rigidBody.SleepTimer >= settings.TimeToSleep) {
						rigidBody.bIsSleeping = true;
						rigidBody.Velocity = {};
						rigidBody.AngularVelocity = 0.0f;
						++stats.SleepingBodies;
					}
				}
			}
		}
		KnownMovingLayers |= movingLayers;
		KnownMovingMasks |= movingMasks;
		StaticTables.clear();
		for (const BodyTable& table : Tables) {
			if (table.Bodies && table.bHasStatics) {
				StaticTables.push_back(table.Source);
			}
		}

	}

	void PhysicsWorld2D::State::FindContacts(float linearSlop)
	{
		Manifolds.clear();
		ManifoldKeys.clear();
		MovingGrid.Build(Bodies, MovingItems);

		auto addPair = [&](uint32_t a, uint32_t b) {
			if (!ShouldCollide(Bodies[a], Bodies[b])) {
				return;
			}
			Manifold manifold;
			if (Collide(Bodies.data(), a, b, linearSlop, manifold)) {
				const SolverBody& bodyA = Bodies[manifold.A];
				const SolverBody& bodyB = Bodies[manifold.B];
				manifold.Friction = std::sqrt(bodyA.Friction * bodyB.Friction);
				manifold.Restitution = std::max(bodyA.Restitution, bodyB.Restitution);
				Manifolds.push_back(manifold);
				ManifoldKeys.push_back(MakePairKey(bodyA.Owner, bodyB.Owner));
			}
		};

		// Awake dynamic bodies against static colliders.
		if (!StaticItems.empty()) {
			for (uint32_t i = 0; i < Bodies.size(); ++i) {
				const SolverBody& body = Bodies[i];
				if (body.bHasShape && body.IsAwakeDynamic()) {
					StaticGrid.Query(Bodies, body, [&](uint32_t other) { addPair(i, other); });
				}
			}
		}

		// Moving bodies against each other (each pair once).
		for (uint32_t i : MovingItems) {
			MovingGrid.Query(Bodies, Bodies[i], [&](uint32_t other) {
				if (other > i) {
					addPair(i, other);
				}
			});
		}
	}

	void PhysicsWorld2D::State::OrderManifolds(const Vector2& gravity)
	{
		// Highest contacts first (against gravity): the weight of a stack then reaches the
		// ground within one iteration, so stacks converge with few iterations.
		const float gravityLength = gravity.Length();
		if (Manifolds.size() < 2 || gravityLength <= 0.0f) {
			return;
		}
		const Vector2 up = gravity / -gravityLength;
		Order.clear();
		for (uint32_t i = 0; i < Manifolds.size(); ++i) {
			const Vector2 middle = (Bodies[Manifolds[i].A].Center + Bodies[Manifolds[i].B].Center) * 0.5f;
			Order.emplace_back(Vector2::Dot(middle, up), i);
		}
		std::sort(Order.begin(), Order.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
		SortedManifolds.clear();
		SortedKeys.clear();
		for (const auto& [height, index] : Order) {
			SortedManifolds.push_back(Manifolds[index]);
			SortedKeys.push_back(ManifoldKeys[index]);
		}
		std::swap(Manifolds, SortedManifolds);
		std::swap(ManifoldKeys, SortedKeys);
	}

	void PhysicsWorld2D::State::WarmStartFromPrevious(float linearSlop)
	{
		const float matchDistanceSquared = 4.0f * linearSlop * linearSlop; // within two slops
		PreviousLookup.Reset(PreviousManifolds.size());
		for (uint32_t i = 0; i < PreviousManifolds.size(); ++i) {
			PreviousLookup.Insert(MakePairKey(PreviousEntities[i * 2], PreviousEntities[i * 2 + 1]), i);
		}
		for (uint32_t m = 0; m < Manifolds.size(); ++m) {
			uint32_t previousIndex = 0;
			if (!PreviousLookup.Find(ManifoldKeys[m], previousIndex)) {
				continue;
			}
			Manifold& manifold = Manifolds[m];
			const Manifold& previous = PreviousManifolds[previousIndex];
			if (PreviousEntities[previousIndex * 2] != Bodies[manifold.A].Owner || PreviousEntities[previousIndex * 2 + 1] != Bodies[manifold.B].Owner
				|| previous.Kind != manifold.Kind) {
				continue;
			}
			// Same features first; otherwise the same spot (a box corner that just crossed the
			// side of the face below changes its feature id while it stays where it was).
			bool bTaken[2] = { false, false };
			for (uint32_t p = 0; p < manifold.PointCount; ++p) {
				int32_t match = -1;
				for (uint32_t q = 0; q < previous.PointCount; ++q) {
					if (!bTaken[q] && previous.Points[q].Id == manifold.Points[p].Id) {
						match = static_cast<int32_t>(q);
						break;
					}
				}
				for (uint32_t q = 0; q < previous.PointCount && match < 0; ++q) {
					if (!bTaken[q] && Vector2::DistanceSquared(previous.Points[q].LocalPointA, manifold.Points[p].LocalPointA) <= matchDistanceSquared) {
						match = static_cast<int32_t>(q);
					}
				}
				if (match >= 0) {
					bTaken[match] = true;
					manifold.Points[p].NormalImpulse = previous.Points[match].NormalImpulse;
					manifold.Points[p].TangentImpulse = previous.Points[match].TangentImpulse;
				}
			}
		}
	}

	void PhysicsWorld2D::State::WakeTouchedBodies()
	{
		// A sleeping body touched by something awake wakes up (one layer of a stack per pass).
		bool bChanged = true;
		while (bChanged) {
			bChanged = false;
			for (const Manifold& manifold : Manifolds) {
				SolverBody& a = Bodies[manifold.A];
				SolverBody& b = Bodies[manifold.B];
				const bool bAwakeA = a.IsAwakeDynamic() || a.IsMovingKinematic();
				const bool bAwakeB = b.IsAwakeDynamic() || b.IsMovingKinematic();
				SolverBody* sleeper = (bAwakeA && b.bSleeping) ? &b : (bAwakeB && a.bSleeping) ? &a : nullptr;
				if (sleeper) {
					sleeper->bSleeping = false;
					sleeper->Body->SleepTimer = 0.0f;
					bChanged = true;
				}
			}
		}
		// Whatever still sleeps doesn't move this step.
		for (SolverBody& body : Bodies) {
			if (body.bSleeping) {
				body.InvMass = 0.0f;
				body.InvInertia = 0.0f;
				body.Velocity = {};
				body.AngularVelocity = 0.0f;
			}
		}
	}

	namespace
	{
		// Solves both normal impulses of a two-point manifold at once: finds x >= 0 with
		// K x + b >= 0 and x (K x + b) = 0 by trying the four cases (Box2D's block solver).
		void SolveBlock(Manifold& manifold, SolverBody& a, SolverBody& b)
		{
			ContactPoint& point1 = manifold.Points[0];
			ContactPoint& point2 = manifold.Points[1];
			const Vector2 normal = manifold.Normal;
			const Vector2 relative1 = b.Velocity + CrossSV(b.AngularVelocity, point1.RB) - a.Velocity - CrossSV(a.AngularVelocity, point1.RA);
			const Vector2 relative2 = b.Velocity + CrossSV(b.AngularVelocity, point2.RB) - a.Velocity - CrossSV(a.AngularVelocity, point2.RA);
			const float old1 = point1.NormalImpulse;
			const float old2 = point2.NormalImpulse;
			// b' = vn - bias - K * old
			const float b1 = Vector2::Dot(relative1, normal) - point1.VelocityBias - (manifold.K11 * old1 + manifold.K12 * old2);
			const float b2 = Vector2::Dot(relative2, normal) - point2.VelocityBias - (manifold.K12 * old1 + manifold.K22 * old2);

			float x1 = 0.0f;
			float x2 = 0.0f;
			// 1. Both points push: x = -inv(K) b
			x1 = -(manifold.InvK11 * b1 + manifold.InvK12 * b2);
			x2 = -(manifold.InvK12 * b1 + manifold.InvK22 * b2);
			if (x1 < 0.0f || x2 < 0.0f) {
				// 2. Only the first point pushes.
				x1 = -point1.NormalMass * b1;
				x2 = 0.0f;
				const float vn2 = manifold.K12 * x1 + b2;
				if (x1 < 0.0f || vn2 < 0.0f) {
					// 3. Only the second point pushes.
					x1 = 0.0f;
					x2 = -point2.NormalMass * b2;
					const float vn1 = manifold.K12 * x2 + b1;
					if (x2 < 0.0f || vn1 < 0.0f) {
						// 4. Neither pushes (if even that fails, keep the old impulses).
						x1 = 0.0f;
						x2 = 0.0f;
						if (b1 < 0.0f || b2 < 0.0f) {
							return;
						}
					}
				}
			}

			const Vector2 impulse1 = normal * (x1 - old1);
			const Vector2 impulse2 = normal * (x2 - old2);
			a.Velocity -= (impulse1 + impulse2) * a.InvMass;
			a.AngularVelocity -= a.InvInertia * (Vector2::Cross(point1.RA, impulse1) + Vector2::Cross(point2.RA, impulse2));
			b.Velocity += (impulse1 + impulse2) * b.InvMass;
			b.AngularVelocity += b.InvInertia * (Vector2::Cross(point1.RB, impulse1) + Vector2::Cross(point2.RB, impulse2));
			point1.NormalImpulse = x1;
			point2.NormalImpulse = x2;
		}
	}

	void PhysicsWorld2D::State::SolveVelocities(const PhysicsSettings& settings, float deltaTime, bool bFirstSubstep)
	{
		// Prepare each point and apply the impulses found so far (warm starting).
		for (Manifold& manifold : Manifolds) {
			SolverBody& a = Bodies[manifold.A];
			SolverBody& b = Bodies[manifold.B];
			for (uint32_t p = 0; p < manifold.PointCount; ++p) {
				ContactPoint& point = manifold.Points[p];
				Vector2 normal;
				Vector2 worldPoint;
				float separation = 0.0f;
				GetWorldContact(manifold, a, b, p, normal, worldPoint, separation);
				manifold.Normal = normal;
				const Vector2 tangent{ normal.y, -normal.x };
				point.RA = worldPoint - a.Center;
				point.RB = worldPoint - b.Center;

				const float rnA = Vector2::Cross(point.RA, normal);
				const float rnB = Vector2::Cross(point.RB, normal);
				const float normalK = a.InvMass + b.InvMass + a.InvInertia * rnA * rnA + b.InvInertia * rnB * rnB;
				point.NormalMass = normalK > 0.0f ? 1.0f / normalK : 0.0f;
				const float rtA = Vector2::Cross(point.RA, tangent);
				const float rtB = Vector2::Cross(point.RB, tangent);
				const float tangentK = a.InvMass + b.InvMass + a.InvInertia * rtA * rtA + b.InvInertia * rtB * rtB;
				point.TangentMass = tangentK > 0.0f ? 1.0f / tangentK : 0.0f;

				const Vector2 relative = b.Velocity + CrossSV(b.AngularVelocity, point.RB) - a.Velocity - CrossSV(a.AngularVelocity, point.RA);
				const float normalSpeed = Vector2::Dot(relative, normal);
				if (bFirstSubstep) {
					// Bounce from the speed it hit with (later sub-steps keep that target).
					point.BounceSpeed = (normalSpeed < -settings.RestitutionThreshold && manifold.Restitution > 0.0f) ? -manifold.Restitution * normalSpeed : 0.0f;
				}
				if (point.BounceSpeed > 0.0f) {
					point.VelocityBias = point.BounceSpeed;
				} else if (separation > 0.0f) {
					point.VelocityBias = -separation / deltaTime; // still apart (contact margin): may close the gap, no more
				} else {
					point.VelocityBias = 0.0f;
				}
			}

			// Block solver for two points, unless the matrix is badly conditioned (then one point is enough).
			manifold.bBlockSolve = false;
			if (manifold.PointCount == 2) {
				const ContactPoint& point1 = manifold.Points[0];
				const ContactPoint& point2 = manifold.Points[1];
				const float rn1A = Vector2::Cross(point1.RA, manifold.Normal);
				const float rn1B = Vector2::Cross(point1.RB, manifold.Normal);
				const float rn2A = Vector2::Cross(point2.RA, manifold.Normal);
				const float rn2B = Vector2::Cross(point2.RB, manifold.Normal);
				const float massSum = a.InvMass + b.InvMass;
				const float k11 = massSum + a.InvInertia * rn1A * rn1A + b.InvInertia * rn1B * rn1B;
				const float k22 = massSum + a.InvInertia * rn2A * rn2A + b.InvInertia * rn2B * rn2B;
				const float k12 = massSum + a.InvInertia * rn1A * rn2A + b.InvInertia * rn1B * rn2B;
				const float determinant = k11 * k22 - k12 * k12;
				constexpr float MaxConditionNumber = 1000.0f;
				if (k11 * k11 < MaxConditionNumber * determinant) {
					manifold.bBlockSolve = true;
					manifold.K11 = k11;
					manifold.K12 = k12;
					manifold.K22 = k22;
					manifold.InvK11 = k22 / determinant;
					manifold.InvK12 = -k12 / determinant;
					manifold.InvK22 = k11 / determinant;
				} else {
					manifold.PointCount = 1; // the points are (nearly) the same constraint
				}
			}

			// Apply last step's impulses (warm starting).
			const Vector2 tangent{ manifold.Normal.y, -manifold.Normal.x };
			for (uint32_t p = 0; p < manifold.PointCount; ++p) {
				const ContactPoint& point = manifold.Points[p];
				const Vector2 impulse = manifold.Normal * point.NormalImpulse + tangent * point.TangentImpulse;
				a.Velocity -= impulse * a.InvMass;
				a.AngularVelocity -= a.InvInertia * Vector2::Cross(point.RA, impulse);
				b.Velocity += impulse * b.InvMass;
				b.AngularVelocity += b.InvInertia * Vector2::Cross(point.RB, impulse);
			}
		}

		const int32_t iterations = std::max(settings.VelocityIterations, 1);
		for (int32_t iteration = 0; iteration < iterations; ++iteration) {
			for (Manifold& manifold : Manifolds) {
				SolverBody& a = Bodies[manifold.A];
				SolverBody& b = Bodies[manifold.B];
				const Vector2 normal = manifold.Normal; // positions don't change while solving velocities
				const Vector2 tangent{ normal.y, -normal.x };

				// Friction first: it's limited by the normal impulse.
				for (uint32_t p = 0; p < manifold.PointCount && manifold.Friction > 0.0f; ++p) {
					ContactPoint& point = manifold.Points[p];
					const Vector2 relative = b.Velocity + CrossSV(b.AngularVelocity, point.RB) - a.Velocity - CrossSV(a.AngularVelocity, point.RA);
					const float maxFriction = manifold.Friction * point.NormalImpulse;
					const float newImpulse = std::clamp(point.TangentImpulse - point.TangentMass * Vector2::Dot(relative, tangent), -maxFriction, maxFriction);
					const Vector2 impulse = tangent * (newImpulse - point.TangentImpulse);
					point.TangentImpulse = newImpulse;
					a.Velocity -= impulse * a.InvMass;
					a.AngularVelocity -= a.InvInertia * Vector2::Cross(point.RA, impulse);
					b.Velocity += impulse * b.InvMass;
					b.AngularVelocity += b.InvInertia * Vector2::Cross(point.RB, impulse);
				}
				if (manifold.bBlockSolve) {
					SolveBlock(manifold, a, b);
					continue;
				}
				for (uint32_t p = 0; p < manifold.PointCount; ++p) {
					ContactPoint& point = manifold.Points[p];
					const Vector2 relative = b.Velocity + CrossSV(b.AngularVelocity, point.RB) - a.Velocity - CrossSV(a.AngularVelocity, point.RA);
					const float normalSpeed = Vector2::Dot(relative, normal);
					const float newImpulse = std::max(point.NormalImpulse - point.NormalMass * (normalSpeed - point.VelocityBias), 0.0f);
					const Vector2 impulse = normal * (newImpulse - point.NormalImpulse);
					point.NormalImpulse = newImpulse;
					a.Velocity -= impulse * a.InvMass;
					a.AngularVelocity -= a.InvInertia * Vector2::Cross(point.RA, impulse);
					b.Velocity += impulse * b.InvMass;
					b.AngularVelocity += b.InvInertia * Vector2::Cross(point.RB, impulse);
				}
			}
		}
	}

	void PhysicsWorld2D::State::SolvePositions(const PhysicsSettings& settings)
	{
		const float slop = std::max(settings.LinearSlop, 0.0f);
		const int32_t iterations = std::max(settings.PositionIterations, 0);
		for (int32_t iteration = 0; iteration < iterations; ++iteration) {
			float minSeparation = 0.0f;
			for (Manifold& manifold : Manifolds) {
				SolverBody& a = Bodies[manifold.A];
				SolverBody& b = Bodies[manifold.B];
				for (uint32_t p = 0; p < manifold.PointCount; ++p) {
					Vector2 normal;
					Vector2 worldPoint;
					float separation = 0.0f;
					GetWorldContact(manifold, a, b, p, normal, worldPoint, separation);
					minSeparation = std::min(minSeparation, separation);

					const float correction = std::clamp(Baumgarte * (separation + slop), -MaxCorrection, 0.0f);
					const Vector2 rA = worldPoint - a.Center;
					const Vector2 rB = worldPoint - b.Center;
					const float rnA = Vector2::Cross(rA, normal);
					const float rnB = Vector2::Cross(rB, normal);
					const float k = a.InvMass + b.InvMass + a.InvInertia * rnA * rnA + b.InvInertia * rnB * rnB;
					if (k <= 0.0f || correction == 0.0f) {
						continue;
					}
					const Vector2 impulse = normal * (-correction / k);
					a.Center -= impulse * a.InvMass;
					a.Angle -= a.InvInertia * Vector2::Cross(rA, impulse);
					b.Center += impulse * b.InvMass;
					b.Angle += b.InvInertia * Vector2::Cross(rB, impulse);
					a.Rot = Rotation2::FromAngle(a.Angle);
					b.Rot = Rotation2::FromAngle(b.Angle);
				}
			}
			if (minSeparation >= -3.0f * slop) {
				break;
			}
		}
	}

	void PhysicsWorld2D::State::UpdateSleep(const PhysicsSettings& settings, float deltaTime, PhysicsStats& stats)
	{
		const uint32_t count = static_cast<uint32_t>(Bodies.size());
		for (SolverBody& body : Bodies) {
			if (body.IsAwakeDynamic()) {
				UpdateSleepTimer(*body.Body, body.Velocity, body.AngularVelocity, settings, deltaTime);
			}
		}
		if (!settings.bAllowSleep) {
			for (SolverBody& body : Bodies) {
				body.bSleeping = false;
			}
			return;
		}

		// Islands: awake dynamic bodies that touch each other sleep together.
		IslandParents.resize(count);
		IslandMinSleep.assign(count, 1e30f);
		for (uint32_t i = 0; i < count; ++i) {
			IslandParents[i] = static_cast<int32_t>(i);
		}
		for (const Manifold& manifold : Manifolds) {
			SolverBody& a = Bodies[manifold.A];
			SolverBody& b = Bodies[manifold.B];
			if (a.IsMovingKinematic() && b.IsAwakeDynamic()) {
				b.Body->SleepTimer = 0.0f; // carried by something moving
			} else if (b.IsMovingKinematic() && a.IsAwakeDynamic()) {
				a.Body->SleepTimer = 0.0f;
			}
			if (a.IsAwakeDynamic() && b.IsAwakeDynamic()) {
				const int32_t rootA = FindRoot(IslandParents, static_cast<int32_t>(manifold.A));
				const int32_t rootB = FindRoot(IslandParents, static_cast<int32_t>(manifold.B));
				if (rootA != rootB) {
					IslandParents[rootA] = rootB;
				}
			}
		}
		for (uint32_t i = 0; i < count; ++i) {
			if (Bodies[i].IsAwakeDynamic()) {
				float& minSleep = IslandMinSleep[FindRoot(IslandParents, static_cast<int32_t>(i))];
				minSleep = std::min(minSleep, Bodies[i].Body->SleepTimer);
			}
		}
		for (uint32_t i = 0; i < count; ++i) {
			SolverBody& body = Bodies[i];
			if (body.IsAwakeDynamic() && IslandMinSleep[FindRoot(IslandParents, static_cast<int32_t>(i))] >= settings.TimeToSleep) {
				body.bSleeping = true;
				body.Velocity = {};
				body.AngularVelocity = 0.0f;
			}
			if (body.Type == BodyType::Dynamic && body.bSleeping) {
				++stats.SleepingBodies;
			}
		}
	}

	void PhysicsWorld2D::State::KeepManifolds()
	{
		PreviousEntities.resize(Manifolds.size() * 2);
		for (uint32_t i = 0; i < Manifolds.size(); ++i) {
			PreviousEntities[i * 2] = Bodies[Manifolds[i].A].Owner;
			PreviousEntities[i * 2 + 1] = Bodies[Manifolds[i].B].Owner;
		}
		std::swap(PreviousManifolds, Manifolds);
	}

	PhysicsStats PhysicsWorld2D::Step(World& world, float deltaTime, const PhysicsSettings& settings)
	{
		PhysicsStats stats;
		if (deltaTime <= 0.0f) {
			return stats;
		}
		State& state = *Data;
		state.Gather(world, settings, deltaTime, stats); // also moves the bodies that touch nothing
		state.FindContacts(std::max(settings.LinearSlop, 0.0f));
		state.OrderManifolds(settings.Gravity);
		state.WarmStartFromPrevious(std::max(settings.LinearSlop, 0.0f));
		state.WakeTouchedBodies();
		// Sub-steps: the same contacts solved over shorter time steps, which makes stacks
		// much stiffer than more iterations would (the impulses carry over between sub-steps).
		const int32_t substeps = std::clamp(settings.Substeps, 1, 16);
		const float substepTime = deltaTime / static_cast<float>(substeps);
		for (int32_t substep = 0; substep < substeps; ++substep) {
			for (SolverBody& body : state.Bodies) {
				if (body.IsAwakeDynamic()) {
					IntegrateVelocity(body, settings, substepTime);
				}
			}
			state.SolveVelocities(settings, substepTime, substep == 0);
			for (SolverBody& body : state.Bodies) {
				if (body.IsAwakeDynamic() || body.Type == BodyType::Kinematic) {
					IntegratePosition(body, substepTime);
				}
			}
		}
		state.SolvePositions(settings);
		state.UpdateSleep(settings, deltaTime, stats);

		for (const SolverBody& body : state.Bodies) {
			if (body.TransformData) {
				WriteBody(body);
			}
		}
		for (const Manifold& manifold : state.Manifolds) {
			stats.Contacts += manifold.PointCount;
		}
		state.KeepManifolds();
		return stats;
	}
}
