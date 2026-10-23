#include "Core/Physics/Contacts2D.h"
#include <algorithm>
#include <cmath>

namespace Flex::Physics
{
	namespace
	{
		// A box as a polygon in world space. Edge i goes from Vertices[i] to Vertices[i + 1].
		struct WorldBox
		{
			Vector2 Vertices[4];
			Vector2 Normals[4];
		};

		WorldBox MakeWorldBox(const SolverBody& body)
		{
			const Vector2 axisX{ body.Rot.Cos, body.Rot.Sin };
			const Vector2 axisY{ -body.Rot.Sin, body.Rot.Cos };
			const Vector2 x = axisX * body.HalfSize.x;
			const Vector2 y = axisY * body.HalfSize.y;
			WorldBox box;
			box.Vertices[0] = body.Center - x - y;
			box.Vertices[1] = body.Center + x - y;
			box.Vertices[2] = body.Center + x + y;
			box.Vertices[3] = body.Center - x + y;
			box.Normals[0] = -axisY;
			box.Normals[1] = axisX;
			box.Normals[2] = axisY;
			box.Normals[3] = -axisX;
			return box;
		}

		// The edge of a that separates the boxes most (largest separation, negative = overlap).
		float FindMaxSeparation(const WorldBox& a, const WorldBox& b, uint32_t& outEdge)
		{
			float best = -1e30f;
			outEdge = 0;
			for (uint32_t i = 0; i < 4; ++i) {
				float deepest = 1e30f;
				for (uint32_t j = 0; j < 4; ++j) {
					deepest = std::min(deepest, Vector2::Dot(a.Normals[i], b.Vertices[j] - a.Vertices[i]));
				}
				if (deepest > best) {
					best = deepest;
					outEdge = i;
				}
			}
			return best;
		}

		struct ClipVertex
		{
			Vector2 Point;
			uint32_t Id = 0;
		};

		// Id bits: 0-1 incident vertex or reference vertex, 4 = clipped, 5-6 incident edge,
		// 8-9 reference edge, 12 = B is the reference box.
		constexpr uint32_t ClippedBit = 1u << 4;

		// Keeps the part of the segment on the inner side of a plane (dot(normal, p) <= offset).
		uint32_t ClipSegment(ClipVertex out[2], const ClipVertex in[2], const Vector2& normal, float offset, uint32_t clipId)
		{
			uint32_t count = 0;
			const float distance0 = Vector2::Dot(normal, in[0].Point) - offset;
			const float distance1 = Vector2::Dot(normal, in[1].Point) - offset;
			if (distance0 <= 0.0f) {
				out[count++] = in[0];
			}
			if (distance1 <= 0.0f) {
				out[count++] = in[1];
			}
			if (distance0 * distance1 < 0.0f && count < 2) {
				const float t = distance0 / (distance0 - distance1);
				out[count].Point = in[0].Point + (in[1].Point - in[0].Point) * t;
				out[count].Id = clipId;
				++count;
			}
			return count;
		}

		bool CollideBoxes(const SolverBody* bodies, uint32_t indexA, uint32_t indexB, float linearSlop, Manifold& manifold)
		{
			const WorldBox boxA = MakeWorldBox(bodies[indexA]);
			const WorldBox boxB = MakeWorldBox(bodies[indexB]);
			uint32_t edgeA = 0;
			const float separationA = FindMaxSeparation(boxA, boxB, edgeA);
			if (separationA > linearSlop) {
				return false;
			}
			uint32_t edgeB = 0;
			const float separationB = FindMaxSeparation(boxB, boxA, edgeB);
			if (separationB > linearSlop) {
				return false;
			}

			// Prefer A as the reference so the choice doesn't flip back and forth.
			const bool bFlip = separationB > separationA + 0.1f * linearSlop;
			const WorldBox& reference = bFlip ? boxB : boxA;
			const WorldBox& incident = bFlip ? boxA : boxB;
			const uint32_t referenceEdge = bFlip ? edgeB : edgeA;
			const Vector2 normal = reference.Normals[referenceEdge];

			// The incident edge is the one facing the reference face most.
			uint32_t incidentEdge = 0;
			float minDot = 1e30f;
			for (uint32_t i = 0; i < 4; ++i) {
				const float dot = Vector2::Dot(normal, incident.Normals[i]);
				if (dot < minDot) {
					minDot = dot;
					incidentEdge = i;
				}
			}
			const uint32_t incident1 = incidentEdge;
			const uint32_t incident2 = (incidentEdge + 1) % 4;
			const uint32_t edgeBits = (incidentEdge << 5) | (referenceEdge << 8) | (bFlip ? (1u << 12) : 0u);
			const ClipVertex incidentSegment[2] = {
				{ incident.Vertices[incident1], incident1 | edgeBits },
				{ incident.Vertices[incident2], incident2 | edgeBits },
			};

			// Clip the incident edge to the sides of the reference face.
			const uint32_t reference1 = referenceEdge;
			const uint32_t reference2 = (referenceEdge + 1) % 4;
			const Vector2 v1 = reference.Vertices[reference1];
			const Vector2 v2 = reference.Vertices[reference2];
			const Vector2 tangent = (v2 - v1).Normalized();
			ClipVertex clipped1[2];
			ClipVertex clipped2[2];
			if (ClipSegment(clipped1, incidentSegment, -tangent, -Vector2::Dot(tangent, v1), reference1 | ClippedBit | edgeBits) < 2) {
				return false;
			}
			if (ClipSegment(clipped2, clipped1, tangent, Vector2::Dot(tangent, v2), reference2 | ClippedBit | edgeBits) < 2) {
				return false;
			}

			const uint32_t a = bFlip ? indexB : indexA;
			const uint32_t b = bFlip ? indexA : indexB;
			const SolverBody& bodyA = bodies[a];
			const SolverBody& bodyB = bodies[b];
			manifold.A = a;
			manifold.B = b;
			manifold.Kind = ManifoldKind::BoxBox;
			manifold.LocalNormal = bodyA.Rot.InverseRotate(normal);
			manifold.PointCount = 0;
			const float front = Vector2::Dot(normal, v1);
			for (const ClipVertex& vertex : clipped2) {
				const float separation = Vector2::Dot(normal, vertex.Point) - front;
				if (separation > linearSlop) {
					continue; // farther apart than the contact margin
				}
				ContactPoint& point = manifold.Points[manifold.PointCount++];
				point = {};
				point.LocalPointA = bodyA.Rot.InverseRotate(vertex.Point - normal * separation - bodyA.Center);
				point.LocalPointB = bodyB.Rot.InverseRotate(vertex.Point - bodyB.Center);
				point.Id = vertex.Id;
			}
			return manifold.PointCount > 0;
		}

		bool CollideBoxCircle(const SolverBody* bodies, uint32_t boxIndex, uint32_t circleIndex, float margin, Manifold& manifold)
		{
			const SolverBody& box = bodies[boxIndex];
			const SolverBody& circle = bodies[circleIndex];
			const Vector2 local = box.Rot.InverseRotate(circle.Center - box.Center);
			const Vector2 clamped{ std::clamp(local.x, -box.HalfSize.x, box.HalfSize.x), std::clamp(local.y, -box.HalfSize.y, box.HalfSize.y) };

			Vector2 normal;
			Vector2 facePoint;
			if (clamped == local) {
				// Centre inside the box: leave through the nearest face.
				const float signX = local.x < 0.0f ? -1.0f : 1.0f;
				const float signY = local.y < 0.0f ? -1.0f : 1.0f;
				const float distanceX = box.HalfSize.x - std::abs(local.x);
				const float distanceY = box.HalfSize.y - std::abs(local.y);
				if (distanceX < distanceY) {
					normal = { signX, 0.0f };
					facePoint = { signX * box.HalfSize.x, local.y };
				} else {
					normal = { 0.0f, signY };
					facePoint = { local.x, signY * box.HalfSize.y };
				}
			} else {
				const Vector2 delta = local - clamped;
				const float distanceSquared = delta.LengthSquared();
				if (distanceSquared > (circle.Radius + margin) * (circle.Radius + margin)) {
					return false;
				}
				normal = delta / std::sqrt(distanceSquared);
				facePoint = clamped;
			}

			manifold.A = boxIndex;
			manifold.B = circleIndex;
			manifold.Kind = ManifoldKind::BoxCircle;
			manifold.LocalNormal = normal;
			manifold.PointCount = 1;
			manifold.Points[0] = {};
			manifold.Points[0].LocalPointA = facePoint;
			return true;
		}

		bool CollideCircles(const SolverBody* bodies, uint32_t indexA, uint32_t indexB, float margin, Manifold& manifold)
		{
			const SolverBody& a = bodies[indexA];
			const SolverBody& b = bodies[indexB];
			const float radius = a.Radius + b.Radius + margin;
			if (Vector2::DistanceSquared(a.Center, b.Center) > radius * radius) {
				return false;
			}
			manifold.A = indexA;
			manifold.B = indexB;
			manifold.Kind = ManifoldKind::CircleCircle;
			manifold.PointCount = 1;
			manifold.Points[0] = {};
			return true;
		}
	}

	bool Collide(const SolverBody* bodies, uint32_t indexA, uint32_t indexB, float linearSlop, Manifold& outManifold)
	{
		const bool bCircleA = bodies[indexA].bIsCircle;
		const bool bCircleB = bodies[indexB].bIsCircle;
		if (!bCircleA && !bCircleB) {
			return CollideBoxes(bodies, indexA, indexB, linearSlop, outManifold);
		}
		if (bCircleA && bCircleB) {
			return CollideCircles(bodies, indexA, indexB, linearSlop, outManifold);
		}
		return bCircleA ? CollideBoxCircle(bodies, indexB, indexA, linearSlop, outManifold) : CollideBoxCircle(bodies, indexA, indexB, linearSlop, outManifold);
	}

	void GetWorldContact(const Manifold& manifold, const SolverBody& a, const SolverBody& b, uint32_t pointIndex,
		Vector2& outNormal, Vector2& outPoint, float& outSeparation)
	{
		const ContactPoint& point = manifold.Points[pointIndex];
		switch (manifold.Kind) {
		case ManifoldKind::BoxBox: {
			outNormal = a.Rot.Rotate(manifold.LocalNormal);
			const Vector2 planePoint = a.Center + a.Rot.Rotate(point.LocalPointA);
			const Vector2 pointB = b.Center + b.Rot.Rotate(point.LocalPointB);
			outSeparation = Vector2::Dot(pointB - planePoint, outNormal);
			outPoint = pointB - outNormal * (0.5f * outSeparation);
			break;
		}
		case ManifoldKind::BoxCircle: {
			outNormal = a.Rot.Rotate(manifold.LocalNormal);
			const Vector2 planePoint = a.Center + a.Rot.Rotate(point.LocalPointA);
			const Vector2 pointB = b.Center - outNormal * b.Radius;
			outSeparation = Vector2::Dot(pointB - planePoint, outNormal);
			outPoint = pointB - outNormal * (0.5f * outSeparation);
			break;
		}
		case ManifoldKind::CircleCircle: {
			const Vector2 delta = b.Center - a.Center;
			const float distance = delta.Length();
			outNormal = distance > Math::Epsilon ? delta / distance : Vector2{ 0.0f, 1.0f };
			outSeparation = distance - a.Radius - b.Radius;
			const Vector2 pointA = a.Center + outNormal * a.Radius;
			const Vector2 pointB = b.Center - outNormal * b.Radius;
			outPoint = (pointA + pointB) * 0.5f;
			break;
		}
		}
	}
}
