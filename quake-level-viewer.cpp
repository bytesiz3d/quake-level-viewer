#include <raylib.h>
#include <raymath.h>
#include <rcamera.h>
#include <rlgl.h>

#include <algorithm>
#include <assert.h>
#include <format>
#include <fstream>
#include <iomanip>
#include <istream>
#include <numeric>
#include <span>
#include <sstream>
#include <unordered_map>

#include "raymath-extras.h"

#if defined(PLATFORM_DESKTOP)
#define GLSL_VERSION 330
#else // PLATFORM_RPI, PLATFORM_ANDROID, PLATFORM_WEB
#define GLSL_VERSION 100
#endif

struct Poly
{
	std::vector<Vector3> v;
	BoundingBox bbox;
};

struct Brush
{
	std::vector<Plane> planes;
	std::vector<Poly> polys;
	BoundingBox bbox;
	Model model;
};

struct Entity
{
	std::unordered_map<std::string, std::string> tags;
	std::vector<Brush> brushes;
};

struct Map
{
	std::vector<Entity> entities;
};

std::vector<Poly>
PolysFromPlanes(std::span<Plane> planes);

Vector3
ReadVector3(std::istream& stream);

Plane
ReadPlane(std::istream& stream);

Brush
ReadBrush(std::istream& stream);

std::pair<std::string, std::string>
ReadProperty(std::istream& stream);

Entity
ReadEntity(std::istream& stream);

Map
ReadMap(std::istream& stream);

Map
LoadMapFile(const char* path)
{
	std::ifstream mapFile{path};
	Map map = ReadMap(mapFile);
	for (auto& entity : map.entities)
	{
		for (auto& brush : entity.brushes)
		{
			brush.polys = PolysFromPlanes(brush.planes);

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

			auto size = BboxSize(brush.bbox);
			auto center = BboxCenter(brush.bbox);
			brush.model = LoadModelFromMesh(GenMeshCube(size.x, size.y, size.z));
		}
	}
	return map;
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

int
main()
{
	SetTargetFPS(60);
	SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
	InitWindow(1600, 900, "quake-level-viewer");
	DisableCursor(); // Limit cursor to relative movement inside the window

	Shader shader = LoadShader(QUAD_VS_PATH, QUAD_FS_PATH);
	int cameraPositionLoc = GetShaderLocation(shader, "cameraPosition");

	auto map = LoadMapFile(MAP_SOURCE_DIR "/DM1.MAP");

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
		SetShaderValue(shader, cameraPositionLoc, &camera.position, SHADER_UNIFORM_VEC3);

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
						auto center = BboxCenter(brush.bbox);
						brush.model.materials[0].shader = shader;
						DrawModel(brush.model, center, 1.f, WHITE);
						DrawCubeWiresV(center, BboxSize(brush.bbox), LIME);
					}
				}
				// DrawGrid(100 + (int)(Vector3Length(camera.position) / 10), 10.f);
				// DrawRay({.direction = {1, 0, 0}}, RED);
				// DrawRay({.direction = {0, 1, 0}}, GREEN);Shader
				// DrawRay({.direction = {0, 0, 1}}, BLUE);
			}
			EndMode3D();

			DrawFPS(10, 10);

			DrawText("WASD: Move, Mouse: Pan, SPACE, CTRL: Up/Down", 10, GetScreenHeight() - 30, 20, DARKGRAY);
		}
		EndDrawing();
	}

	UnloadShader(shader);
	CloseWindow();
	return 0;
}

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
					polys[i].v.push_back(vertex);
					polys[j].v.push_back(vertex);
					polys[k].v.push_back(vertex);
				}
			}
		}
	}

	for (auto& poly : polys)
	{
		if (poly.v.empty())
			continue;

		Vector3 minVertex = poly.v[0];
		Vector3 maxVertex = poly.v[0];

		for (size_t i = 1; i < poly.v.size(); i++)
		{
			minVertex = Vector3Min(minVertex, poly.v[i]);
			maxVertex = Vector3Max(maxVertex, poly.v[i]);
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

	Brush brush = {planes};
	return brush;
}

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
