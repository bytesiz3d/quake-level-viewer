#include <raylib.h>
#include <raymath.h>
#include <rcamera.h>
#include <rlgl.h>

#include <assert.h>
#include <format>
#include <numeric>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <istream>
#include <span>
#include <sstream>
#include <unordered_map>

#include "raymath-extras.h"

#if defined(PLATFORM_DESKTOP)
#define GLSL_VERSION 330
#else // PLATFORM_RPI, PLATFORM_ANDROID, PLATFORM_WEB
#define GLSL_VERSION 100
#endif

struct Vertex
{
	Vector3 p;
	Vector3 n;
};

struct Poly
{
	std::vector<Vertex> v;
	BoundingBox bbox;
};

std::vector<Poly>
PolysFromPlanes(std::span<Plane> planes)
{
	std::vector<Poly> polys(planes.size());
	for (size_t i = 0; i < planes.size() - 2; i++)
	{
		for (size_t j = i + 1; j < planes.size() - 1; j++)
		{
			for (size_t k = j + 1; k < planes.size(); k++)
			{
				bool legal = true;

				Vector3 vertex = {};
				if (GetIntersectionPlanes(planes[i], planes[j], planes[k], vertex) == false)
					continue;

				for (size_t m = 0; m < planes.size(); m++)
				{
					if (PlaneSignedDistance(planes[m], vertex) > 0)
					{
						legal = false;
						break;
					}
				}
				if (legal)
				{
					polys[i].v.push_back({vertex, planes[i].n});
					polys[j].v.push_back({vertex, planes[j].n});
					polys[k].v.push_back({vertex, planes[k].n});
				}
			}
		}
	}

	for (auto& poly : polys)
	{
		if (poly.v.empty())
			continue;

		Vector3 minVertex = poly.v[0].p;
		Vector3 maxVertex = poly.v[0].p;

		for (size_t i = 1; i < poly.v.size(); i++)
		{
			minVertex = Vector3Min(minVertex, poly.v[i].p);
			maxVertex = Vector3Max(maxVertex, poly.v[i].p);
		}
		poly.bbox = {.min = minVertex, .max = maxVertex};
	}

	return polys;
}

Vector3
ReadVector3(std::istream& stream)
{
	Vector3 v;

	char token;
	stream >> token;
	if (token != '(')
		throw std::format("Expected '(', found {}", token);

	stream >> v.x >> v.y >> v.z;

	stream >> token;
	if (token != ')')
		throw std::format("Expected ')', found {}", token);

	// IDEA: Divide components by scale?
	// Changing from Quake's (left-handed, Z-Up) system to Raylib's (right-handed, Y-Up) system
	return {v.x, v.z, -v.y};
}

Plane
ReadPlane(std::istream& stream)
{
	char token;

	Vector3 pv[3];
	for (auto& v : pv)
		v = ReadVector3(stream);

	// Skip over remaining data until the next line
	char buf[256];
	stream.getline(buf, sizeof(buf));

	// Changing from Quake's (left-handed, Z-Up) system to Raylib's (right-handed, Y-Up) system
	return PlaneFromPoints(pv[0], pv[2], pv[1]);
}

struct Brush
{
	std::vector<Plane> planes;
	std::vector<Poly> polys;
	BoundingBox bbox;
};

Brush
ReadBrush(std::istream& stream)
{
	std::vector<Plane> planes;

	char token;
	stream >> token;
	if (token != '{')
		throw std::format("Expected '{{', found {}", token);

	while (stream >> std::ws)
	{
		token = stream.peek();
		if (token == '(')
		{
			auto plane = ReadPlane(stream);
			planes.push_back(plane);
		}
		else if (token == '}')
		{
			stream >> token;
			break;
		}
		else
			throw std::format("Unexpected token {}", token);
	}

	Brush brush = {planes, PolysFromPlanes(planes)};
	Vector3 minVertex = brush.polys[0].bbox.min;
	Vector3 maxVertex = brush.polys[0].bbox.max;
	for (auto& poly : brush.polys)
	{
		if (poly.v.empty())
			continue;

		minVertex = Vector3Min(minVertex, poly.bbox.min);
		maxVertex = Vector3Max(maxVertex, poly.bbox.max);
	}
	brush.bbox = {minVertex, maxVertex};
	return brush;
}

struct Entity
{
	std::unordered_map<std::string, std::string> tags;
	std::vector<Brush> brushes;
};

std::pair<std::string, std::string>
ReadProperty(std::istream& stream)
{
	std::string tag, tagValue;
	stream >> std::quoted(tag) >> std::quoted(tagValue);
	return {tag, tagValue};
}

Entity
ReadEntity(std::istream& stream)
{
	Entity entity{};

	char token;
	stream >> token;
	if (token != '{')
		throw std::format("Expected '{{', found {}", token);

	while (stream >> std::ws)
	{
		token = stream.peek();
		if (token == '"')
		{
			auto [tag, tagValue] = ReadProperty(stream);
			entity.tags[tag] = tagValue;
		}
		else if (token == '{')
		{
			entity.brushes.push_back(ReadBrush(stream));
		}
		else if (token == '}')
		{
			stream >> token;
			break;
		}
		else
			throw std::format("Unexpected token {}", token);
	}

	return entity;
}


struct Map
{
	std::vector<Entity> entities;
};

Map
ReadMap(std::istream& stream)
{
	Map map = {};
	while ((stream >> std::ws).good())
	{
		Entity entity = ReadEntity(stream);
		assert(entity.tags.empty() == false);
		map.entities.push_back(entity);
	}
	return map;
}

Map
LoadMapFile(const char* path)
{
	std::ifstream mapFile{path};
	return ReadMap(mapFile);
}

void
UpdateCamera(Camera* camera)
{
	float CAMERA_MOVE_SPEED = 0.03f;
	constexpr float CAMERA_ROTATION_SPEED = 0.03f;
	constexpr float CAMERA_MOUSE_MOVE_SENSITIVITY = 0.003f;

	if (IsKeyDown(KEY_LEFT_SHIFT))
		CAMERA_MOVE_SPEED *= 10;

	Vector2 mousePositionDelta = GetMouseDelta();

	bool lockView = true;
	bool rotateAroundTarget = false;
	bool rotateUp = false;
	bool moveInWorldPlane = true;

	// Camera rotation
	if (IsKeyDown(KEY_DOWN)) CameraPitch(camera, -CAMERA_ROTATION_SPEED, lockView, rotateAroundTarget, rotateUp);
	if (IsKeyDown(KEY_UP)) CameraPitch(camera, CAMERA_ROTATION_SPEED, lockView, rotateAroundTarget, rotateUp);
	if (IsKeyDown(KEY_RIGHT)) CameraYaw(camera, -CAMERA_ROTATION_SPEED, rotateAroundTarget);
	if (IsKeyDown(KEY_LEFT)) CameraYaw(camera, CAMERA_ROTATION_SPEED, rotateAroundTarget);
	// if (IsKeyDown(KEY_Q)) CameraRoll(camera, -CAMERA_ROTATION_SPEED);
	// if (IsKeyDown(KEY_E)) CameraRoll(camera, CAMERA_ROTATION_SPEED);

	CameraYaw(camera, -mousePositionDelta.x * CAMERA_MOUSE_MOVE_SENSITIVITY, rotateAroundTarget);
	CameraPitch(camera, -mousePositionDelta.y * CAMERA_MOUSE_MOVE_SENSITIVITY, lockView, rotateAroundTarget, rotateUp);

	// Camera movement
	if (IsKeyDown(KEY_W)) CameraMoveForward(camera, 20 * CAMERA_MOVE_SPEED, moveInWorldPlane);
	if (IsKeyDown(KEY_A)) CameraMoveRight(camera, -20 * CAMERA_MOVE_SPEED, moveInWorldPlane);
	if (IsKeyDown(KEY_S)) CameraMoveForward(camera, -20 * CAMERA_MOVE_SPEED, moveInWorldPlane);
	if (IsKeyDown(KEY_D)) CameraMoveRight(camera, 20 * CAMERA_MOVE_SPEED, moveInWorldPlane);
	if (IsKeyDown(KEY_SPACE)) CameraMoveUp(camera, 20 * CAMERA_MOVE_SPEED);
	if (IsKeyDown(KEY_LEFT_CONTROL)) CameraMoveUp(camera, -20 * CAMERA_MOVE_SPEED);
}

float
AttenuatedLight(float distance)
{
	constexpr float HD = 30.f;
	constexpr float K = 1.f / (HD * HD);
	return 1.f / (1.f + K * distance * distance);
}

void
DrawBboxVertex(float x, float y, float z, Vector3 cameraPosition)
{
	float distance = Vector3Distance(cameraPosition, {x, y, z});
	float attenuated_light = AttenuatedLight(distance);
	rlColor3f(attenuated_light, attenuated_light, attenuated_light);
	rlVertex3f(x, y, z);
}

void
DrawBbox(BoundingBox bbox, Vector3 cameraPosition)
{
	Vector3 position = BboxCenter(bbox);
	float width = BboxSize(bbox).x;
	float height = BboxSize(bbox).y;
	float length = BboxSize(bbox).z;

	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;

	rlPushMatrix();
	// NOTE: Transformation is applied in inverse order (scale -> rotate -> translate)
	rlTranslatef(position.x, position.y, position.z);
	//rlRotatef(45, 0, 1, 0);
	//rlScalef(1.0f, 1.0f, 1.0f);   // NOTE: Vertices are directly scaled on definition

	rlBegin(RL_TRIANGLES);

	// Front face
	DrawBboxVertex(x - width / 2, y - height / 2, z + length / 2, cameraPosition); // Bottom Left
	DrawBboxVertex(x + width / 2, y - height / 2, z + length / 2, cameraPosition); // Bottom Right
	DrawBboxVertex(x - width / 2, y + height / 2, z + length / 2, cameraPosition); // Top Left

	DrawBboxVertex(x + width / 2, y + height / 2, z + length / 2, cameraPosition); // Top Right
	DrawBboxVertex(x - width / 2, y + height / 2, z + length / 2, cameraPosition); // Top Left
	DrawBboxVertex(x + width / 2, y - height / 2, z + length / 2, cameraPosition); // Bottom Right

	// Back face
	DrawBboxVertex(x - width / 2, y - height / 2, z - length / 2, cameraPosition); // Bottom Left
	DrawBboxVertex(x - width / 2, y + height / 2, z - length / 2, cameraPosition); // Top Left
	DrawBboxVertex(x + width / 2, y - height / 2, z - length / 2, cameraPosition); // Bottom Right

	DrawBboxVertex(x + width / 2, y + height / 2, z - length / 2, cameraPosition); // Top Right
	DrawBboxVertex(x + width / 2, y - height / 2, z - length / 2, cameraPosition); // Bottom Right
	DrawBboxVertex(x - width / 2, y + height / 2, z - length / 2, cameraPosition); // Top Left

	// Top face
	DrawBboxVertex(x - width / 2, y + height / 2, z - length / 2, cameraPosition); // Top Left
	DrawBboxVertex(x - width / 2, y + height / 2, z + length / 2, cameraPosition); // Bottom Left
	DrawBboxVertex(x + width / 2, y + height / 2, z + length / 2, cameraPosition); // Bottom Right

	DrawBboxVertex(x + width / 2, y + height / 2, z - length / 2, cameraPosition); // Top Right
	DrawBboxVertex(x - width / 2, y + height / 2, z - length / 2, cameraPosition); // Top Left
	DrawBboxVertex(x + width / 2, y + height / 2, z + length / 2, cameraPosition); // Bottom Right

	// Bottom face
	DrawBboxVertex(x - width / 2, y - height / 2, z - length / 2, cameraPosition); // Top Left
	DrawBboxVertex(x + width / 2, y - height / 2, z + length / 2, cameraPosition); // Bottom Right
	DrawBboxVertex(x - width / 2, y - height / 2, z + length / 2, cameraPosition); // Bottom Left

	DrawBboxVertex(x + width / 2, y - height / 2, z - length / 2, cameraPosition); // Top Right
	DrawBboxVertex(x + width / 2, y - height / 2, z + length / 2, cameraPosition); // Bottom Right
	DrawBboxVertex(x - width / 2, y - height / 2, z - length / 2, cameraPosition); // Top Left

	// Right face
	DrawBboxVertex(x + width / 2, y - height / 2, z - length / 2, cameraPosition); // Bottom Right
	DrawBboxVertex(x + width / 2, y + height / 2, z - length / 2, cameraPosition); // Top Right
	DrawBboxVertex(x + width / 2, y + height / 2, z + length / 2, cameraPosition); // Top Left

	DrawBboxVertex(x + width / 2, y - height / 2, z + length / 2, cameraPosition); // Bottom Left
	DrawBboxVertex(x + width / 2, y - height / 2, z - length / 2, cameraPosition); // Bottom Right
	DrawBboxVertex(x + width / 2, y + height / 2, z + length / 2, cameraPosition); // Top Left

	// Left face
	DrawBboxVertex(x - width / 2, y - height / 2, z - length / 2, cameraPosition); // Bottom Right
	DrawBboxVertex(x - width / 2, y + height / 2, z + length / 2, cameraPosition); // Top Left
	DrawBboxVertex(x - width / 2, y + height / 2, z - length / 2, cameraPosition); // Top Right

	DrawBboxVertex(x - width / 2, y - height / 2, z + length / 2, cameraPosition); // Bottom Left
	DrawBboxVertex(x - width / 2, y + height / 2, z + length / 2, cameraPosition); // Top Left
	DrawBboxVertex(x - width / 2, y - height / 2, z - length / 2, cameraPosition); // Bottom Right

	rlEnd();
	rlPopMatrix();
}

float
GetEdgeDistance(Vector3 e1, Vector3 e2, Vector3 v)
{
	float component = dot(v - e1, e2 - e1);
	if (0 <= component && component <= Vector3Length(e2 - e1))
	{
		Vector3 edgeDirection = Vector3Normalize(e2 - e1);
		Vector3 projection = e1 + component * edgeDirection;
		return Vector3Distance(v, projection);
	}
	return FLT_MAX;
}

float
GetQuadDistance(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Vector3 v)
{
	float distance[9] = {FLT_MAX};

	Plane quadPlane = PlaneFromPoints(a, b, c);
	RayCollision hit = GetRayCollisionQuad({.position = v, .direction = -quadPlane.n}, a, b, c, d);
	if (hit.hit)
		distance[0] = hit.distance; // inside quad

	// inside edge
	distance[1] = GetEdgeDistance(a, b, v);
	distance[2] = GetEdgeDistance(b, c, v);
	distance[3] = GetEdgeDistance(c, d, v);
	distance[4] = GetEdgeDistance(d, a, v);

	// nearest vertex
	distance[5] = Vector3Distance(a, v);
	distance[6] = Vector3Distance(b, v);
	distance[7] = Vector3Distance(c, v);
	distance[8] = Vector3Distance(d, v);

	return *std::min_element(std::begin(distance), std::end(distance));
}

float
GetBboxDistance(BoundingBox bbox, Vector3 v)
{
	Vector3 d = bbox.max - bbox.min;
	Vector3
		v000 = bbox.min + Vector3{0, 0, 0},
		v100 = bbox.min + Vector3{d.x, 0, 0},
		v110 = bbox.min + Vector3{d.x, d.y, 0},
		v010 = bbox.min + Vector3{0, d.y, 0},

		v001 = bbox.min + Vector3{0, 0, d.z},
		v101 = bbox.min + Vector3{d.x, 0, d.z},
		v111 = bbox.min + Vector3{d.x, d.y, d.z},
		v011 = bbox.min + Vector3{0, d.y, d.z};

	float distance[6] = {FLT_MAX};
	distance[0] = GetQuadDistance(v001, v101, v111, v011, v);
	distance[1] = GetQuadDistance(v101, v100, v110, v111, v);
	distance[2] = GetQuadDistance(v100, v000, v010, v110, v);
	distance[3] = GetQuadDistance(v000, v001, v011, v010, v);
	distance[4] = GetQuadDistance(v011, v111, v110, v010, v);
	distance[5] = GetQuadDistance(v000, v100, v101, v001, v);
	return *std::min_element(std::begin(distance), std::end(distance));
}

int
main()
{
	SetTargetFPS(60);
	SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
	InitWindow(1600, 900, "quake-level-viewer");
	DisableCursor(); // Limit cursor to relative movement inside the window

	auto map = LoadMapFile(MAP_SOURCE_DIR "/B_ARMOR1.MAP");

	Camera camera = {
		.position = {10.0f, 10.0f, 10.0f},
		.target = {},
		.up = {0.0f, 1.0f, 0.0f},
		.fovy = 45.0f,
		.projection = CAMERA_PERSPECTIVE,
	};

	rlDisableBackfaceCulling();
	rlEnableDepthMask();
	while (!WindowShouldClose())
	{
		if (IsFileDropped())
		{
			FilePathList droppedFiles = LoadDroppedFiles();
			map = LoadMapFile(droppedFiles.paths[0]);
			UnloadDroppedFiles(droppedFiles);
		}

		static bool cursorEnabled = false;
		if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
		{
			if (cursorEnabled)
			{
				DisableCursor();
				cursorEnabled = false;
			}
			else
			{
				EnableCursor();
				cursorEnabled = true;
			}
		}

		UpdateCamera(&camera);

		float nearestDistance = FLT_MAX;
		BoundingBox nearestPolyBbox;
		BeginDrawing();
		{
			ClearBackground(RAYWHITE);

			BeginMode3D(camera);
			{
				Ray cameraRay = {.position = camera.position};
				for (auto& entity : map.entities)
				{
					for (auto& brush : entity.brushes)
					{
						auto bbox = brush.bbox;

						float polyDistance = GetBboxDistance(bbox, camera.position);
						if (polyDistance < nearestDistance)
						{
							nearestDistance = polyDistance;
							nearestPolyBbox = bbox;
						}

						DrawBoundingBox(bbox, LIME);

						uint8_t grayscale = 255 * AttenuatedLight(polyDistance);
						// DrawCubeV(BboxCenter(bbox), BboxSize(bbox), {grayscale, grayscale, grayscale, 255});
						DrawBbox(bbox, camera.position);
					}
				}
				// DrawGrid(100 + (int)(Vector3Length(camera.position) / 10), 10.f);
				// DrawRay({.direction = {1, 0, 0}}, RED);
				// DrawRay({.direction = {0, 1, 0}}, GREEN);
				// DrawRay({.direction = {0, 0, 1}}, BLUE);

				DrawBoundingBox(nearestPolyBbox, RED);
			}
			EndMode3D();

			DrawFPS(10, 10);

			DrawText(TextFormat("Nearest Distance: %f", nearestDistance), 10, GetScreenHeight() - 60, 20, DARKGRAY);

			DrawText("WASD: Move, Mouse: Pan, SPACE, CTRL: Up/Down", 10, GetScreenHeight() - 30, 20, DARKGRAY);
		}
		EndDrawing();
	}

	CloseWindow();
	return 0;
}