#include <raylib.h>
#include <raymath.h>
#include <rcamera.h>
#include <rlgl.h>

#define RLIGHTS_IMPLEMENTATION
#include <rlights.h>

#include <algorithm>
#include <assert.h>
#include <fstream>
#include <iomanip>
#include <istream>
#include <numeric>
#include <span>
#include <sstream>
#include <unordered_map>

#if defined(PLATFORM_DESKTOP)
#define GLSL_VERSION 330
#else  // PLATFORM_RPI, PLATFORM_ANDROID, PLATFORM_WEB
#define GLSL_VERSION 100
#endif

// n . x + d = 0
struct Plane
{
	Vector3 n;
	float d;
};

struct Poly
{
	std::vector<Vector3> v;
};

struct Brush
{
	std::vector<Plane> planes;
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

Map
ReadMap(std::istream &stream);

Map
LoadMapFile(const char *path)
{
	std::ifstream mapFile{path};
	return ReadMap(mapFile);
}

Model
LoadModelFromMap(const Map &map);

void
UpdateCamera(Camera *camera);

int
main()
{
	// SetTraceLogLevel(LOG_DEBUG);
	SetTargetFPS(60);
	SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
	InitWindow(1200, 800, "quake-level-viewer");
	DisableCursor();  // Limit cursor to relative movement inside the window
	rlDisableBackfaceCulling();
	rlEnableDepthMask();
	
	Camera camera = {
		.position = {10.0f, 10.0f, 10.0f},
		.target = {},
		.up = {0.0f, 1.0f, 0.0f},
		.fovy = 45.0f,
		.projection = CAMERA_PERSPECTIVE,
	};

	Shader shader = LoadShader(VS_PATH, FS_PATH);
	shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");

	Light cameraLight = CreateLight(LIGHT_POINT, camera.position, {}, WHITE, shader);

	auto map = LoadMapFile(MAP_SOURCE_DIR "/DM1.MAP");
	auto model = LoadModelFromMap(map);
	model.materials[0].shader = shader;

	while (!WindowShouldClose())
	{
		static bool enable_lines = true;
		static bool enable_cursor = false;
		
		if (IsFileDropped())
		{
			FilePathList droppedFiles = LoadDroppedFiles();
			map = LoadMapFile(droppedFiles.paths[0]);
			UnloadModel(model);
			model = LoadModelFromMap(map);
			UnloadDroppedFiles(droppedFiles);
		}

		if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
		{
			enable_cursor = !enable_cursor;
			if (enable_cursor)
				EnableCursor();
			else
				DisableCursor();
		}

		if (IsKeyPressed(KEY_L))
			enable_lines = !enable_lines;

		UpdateCamera(&camera);
		cameraLight.position = camera.position;
		UpdateLightValues(shader, cameraLight);

		BeginDrawing();
		{
			ClearBackground(RAYWHITE);

			BeginMode3D(camera);
			{
				DrawModel(model, {}, 1.f, WHITE);

				if (enable_lines)
					DrawModelWires(model, {}, 1.f, BLACK);

				// DrawGrid(100 + (int)(Vector3Length(camera.position) / 10), 10.f);
				// DrawRay({.direction = {1, 0, 0}}, RED);
				// DrawRay({.direction = {0, 1, 0}}, GREEN);
				// DrawRay({.direction = {0, 0, 1}}, BLUE);
			}
			EndMode3D();

			DrawFPS(10, 10);

			DrawText("WASD (+SHIFT): Move, Mouse: Pan, SPACE/CTRL: Up/Down, L: Wireframe, RMB: Cursor", 10, GetScreenHeight() - 30, 20, DARKGRAY);
		}
		EndDrawing();
	}

	UnloadShader(shader);
	UnloadModel(model);
	CloseWindow();
	return 0;
}

inline static Vector3
operator+(Vector3 a, Vector3 b)
{
	return Vector3Add(a, b);
}

inline static Vector3
operator-(Vector3 a, Vector3 b)
{
	return Vector3Subtract(a, b);
}

inline static Vector3
operator*(Vector3 a, float b)
{
	return Vector3Scale(a, b);
}

inline static Vector3
operator*(float a, Vector3 b)
{
	return b * a;
}

inline static Vector3
operator/(Vector3 a, float b)
{
	return Vector3Scale(a, 1.f / b);
}

Vector3
VerticesNormal(std::span<const Vector3> vertices)
{
	Vector3 n = {};
	for (size_t i = 0; i < vertices.size(); i++)
	{
		size_t j = (i + 1) % vertices.size();

		Vector3 vi = vertices[i], vj = vertices[j];
		n.x += (vi.y - vj.y) * (vi.z + vj.z);
		n.y += (vi.z - vj.z) * (vi.x + vj.x);
		n.z += (vi.x - vj.x) * (vi.y + vj.y);
	}
	return Vector3Normalize(n);
}

Plane
PlaneFromVertices(std::span<const Vector3> vertices)
{
	Plane plane = {};
	plane.n = VerticesNormal(vertices);

	if (Vector3Equals(plane.n, {}) || Vector3Length(plane.n) <= EPSILON)
		return Plane{};

	plane.d = -Vector3DotProduct(vertices[0], plane.n);
	return plane;
}

Plane
PlaneFromTriangle(Vector3 v1, Vector3 v2, Vector3 v3)
{
	// Vertices are listed in counter-clockwise order
	// 0 ---------- 2
	// |
	// |
	// 1
	// Plane plane = {};
	// plane.n = Vector3Normalize(Vector3CrossProduct(v2 - v1, v3 - v1));
	// plane.d = -dot(v1, plane.n);
	// return plane;
	Vector3 vertices[3] = {v1, v2, v3};
	return PlaneFromVertices(vertices);
}

float
PlaneSignedDistance(Plane p, Vector3 v)
{
	return Vector3DotProduct(p.n, v) + p.d;
}

std::pair<Vector3, bool>
GetIntersectionPlanes(Plane p1, Plane p2, Plane p3)
{
	float det = Vector3DotProduct(p1.n, Vector3CrossProduct(p2.n, p3.n));
	if (det == 0)
		return {Vector3{}, false};

	Vector3 out = (-p1.d * Vector3CrossProduct(p2.n, p3.n) +
					  -p2.d * Vector3CrossProduct(p3.n, p1.n) +
					  -p3.d * Vector3CrossProduct(p1.n, p2.n)) /
				  det;
	return {out, true};
}

std::vector<Vector3>
PolySortVertices(const Poly &poly, Vector3 planeNormal)
{
	std::vector<Vector3> vertices(poly.v.begin(), poly.v.end());
	Vector3 center = std::reduce(vertices.begin(), vertices.end()) / vertices.size();

	for (auto it = vertices.begin(); (it + 2) != vertices.end(); it++)
	{
		Plane orthogonalPlane = PlaneFromTriangle(*it, center, center + planeNormal);
		Vector3 vN = *it - center;

		auto nearestVertex = std::min_element(it + 1, vertices.end(), [=](Vector3 a, Vector3 b) {
			if (PlaneSignedDistance(orthogonalPlane, a) < 0)  // On opposite sides of the plane from it
				return false;
			if (PlaneSignedDistance(orthogonalPlane, b) < 0)  // On opposite sides of the plane from it
				return true;

			Vector3 vA = a - center, vB = b - center;
			return Vector3Angle(vN, vA) < Vector3Angle(vN, vB);
		});
		std::swap(*(it + 1), *nearestVertex);
	}
	return vertices;
}

std::vector<Poly>
PolysFromPlanes(std::span<const Plane> planes)
{
	std::vector<Poly> polys(planes.size());
	for (size_t i = 0; i < planes.size(); i++)
	{
		for (size_t j = i + 1; j < planes.size(); j++)
		{
			for (size_t k = j + 1; k < planes.size(); k++)
			{
				bool legal = true;

				auto [vertex, ok] = GetIntersectionPlanes(planes[i], planes[j], planes[k]);
				if (ok == false)
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

	for (size_t i = 0; i < polys.size(); i++)
	{
		auto &poly = polys[i];
		auto &plane = planes[i];

		if (poly.v.size() < 3)
			continue;

		poly.v = PolySortVertices(poly, plane.n);
		Vector3 polyNormal = VerticesNormal(poly.v);
		if (Vector3DotProduct(plane.n, polyNormal) < 0)  // opposite sides
			std::reverse(poly.v.begin(), poly.v.end());

		// TODO: Constructive Solid Geometry Union (CSGUnion)
	}

	std::erase_if(polys, [](const Poly &p) { return p.v.size() < 3; });
	return polys;
}

Vector3
ReadVector3(std::istream &stream)
{
	Vector3 v;

	char token;
	stream >> token;
	if (token != '(')
		throw TextFormat("Expected ')', found %c", token);

	stream >> v.x >> v.y >> v.z;

	stream >> token;
	if (token != ')')
		throw TextFormat("Expected ')', found %c", token);

	// IDEA: Divide components by scale?
	// Changing from Quake's (left-handed, Z-Up) system to Raylib's (right-handed, Y-Up) system
	return {v.x, v.z, -v.y};
}

Plane
ReadPlane(std::istream &stream)
{
	char token;

	Vector3 pv[3];
	for (auto &v : pv)
		v = ReadVector3(stream);

	// Skip over remaining data until the next line
	char buf[256];
	stream.getline(buf, sizeof(buf));

	// Changing from Quake's (left-handed, Z-Up) system to Raylib's (right-handed, Y-Up) system
	return PlaneFromTriangle(pv[0], pv[2], pv[1]);
}

Brush
ReadBrush(std::istream &stream)
{
	std::vector<Plane> planes;

	char token;
	stream >> token;
	if (token != '{')
		throw TextFormat("Expected ')', found %c", token);

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
			throw TextFormat("Expected ')', found %c", token);
	}

	Brush brush = {planes};
	return brush;
}

std::pair<std::string, std::string>
ReadProperty(std::istream &stream)
{
	std::string tag, tagValue;
	stream >> std::quoted(tag) >> std::quoted(tagValue);
	return {tag, tagValue};
}

Entity
ReadEntity(std::istream &stream)
{
	Entity entity{};

	char token;
	stream >> token;
	if (token != '{')
		throw TextFormat("Expected ')', found %c", token);

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
			throw TextFormat("Expected ')', found %c", token);
	}

	return entity;
}

Map
ReadMap(std::istream &stream)
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

Mesh
GenMeshPolygons(std::span<const Poly> polys)
{
	Mesh mesh = {};
	for (auto &p : polys)
	{
		mesh.vertexCount += p.v.size();
		mesh.triangleCount += p.v.size() - 2;
	}

	// Vertices
	mesh.vertices = (float *)RL_MALLOC(mesh.vertexCount * 3 * sizeof(float));
	float *vertices = mesh.vertices;
	for (const auto &p : polys)
	{
		for (const auto &v : p.v)
		{
			*(vertices++) = v.x;
			*(vertices++) = v.y;
			*(vertices++) = v.z;
		}
	}

	// Normals
	mesh.normals = (float *)RL_MALLOC(mesh.vertexCount * 3 * sizeof(float));
	float *normals = mesh.normals;
	for (auto &p : polys)
	{
		Vector3 n = VerticesNormal(p.v);
		for (const auto &v : p.v)
		{
			*(normals++) = n.x;
			*(normals++) = n.y;
			*(normals++) = n.z;
		}
	}

	// TexCoords
	mesh.texcoords = (float *)RL_MALLOC(mesh.vertexCount * 2 * sizeof(float));
	float *texcoords = mesh.texcoords;
	for (auto &p : polys)
	{
		for (const auto &v : p.v)
		{
			*(texcoords++) = 0.f;
			*(texcoords++) = 0.f;
		}
	}

	// Indices
	mesh.indices = (unsigned short *)RL_MALLOC(mesh.triangleCount * 3 * sizeof(unsigned short));
	unsigned short *indices = mesh.indices;

	unsigned short pOffset = 0;
	for (auto &p : polys)
	{
		for (unsigned short i = 1; (i + 1) < p.v.size(); i++)
		{
			*(indices++) = pOffset;
			*(indices++) = pOffset + i;
			*(indices++) = pOffset + i + 1;
		}
		pOffset += p.v.size();
	}

	// Upload vertex data to GPU (static mesh)
	UploadMesh(&mesh, false);

	for (unsigned short i = 0; i < mesh.vertexCount; i++)
	{
		TraceLog(LOG_DEBUG, "[%d] v (%.3f %.3f %.3f), tc (%.3f %.3f), n (%.3f %.3f %.3f)",
			i,
			mesh.vertices[3 * i], mesh.vertices[3 * i + 1], mesh.vertices[3 * i + 2],
			mesh.texcoords[2 * i], mesh.texcoords[2 * i + 1],
			mesh.normals[3 * i], mesh.normals[3 * i + 1], mesh.normals[3 * i + 2]);
	}
	for (size_t i = 0; i < mesh.triangleCount; i++)
	{
		TraceLog(LOG_DEBUG, "idx (%d %d %d)",
			mesh.indices[3 * i], mesh.indices[3 * i + 1], mesh.indices[3 * i + 2]);
	}

	return mesh;
}

Model
LoadModelFromMap(const Map &map)
{
	std::vector<Poly> mapPolys;
	for (auto &entity : map.entities)
	{
		for (auto &brush : entity.brushes)
		{
			auto polys = PolysFromPlanes(brush.planes);
			mapPolys.insert(mapPolys.end(), polys.begin(), polys.end());
		}
	}
	return LoadModelFromMesh(GenMeshPolygons(mapPolys));
}

void
UpdateCamera(Camera *camera)
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
	if (IsKeyDown(KEY_DOWN))
		CameraPitch(camera, -CAMERA_ROTATION_SPEED, lockView, rotateAroundTarget, rotateUp);
	if (IsKeyDown(KEY_UP))
		CameraPitch(camera, CAMERA_ROTATION_SPEED, lockView, rotateAroundTarget, rotateUp);
	if (IsKeyDown(KEY_RIGHT))
		CameraYaw(camera, -CAMERA_ROTATION_SPEED, rotateAroundTarget);
	if (IsKeyDown(KEY_LEFT))
		CameraYaw(camera, CAMERA_ROTATION_SPEED, rotateAroundTarget);
	// if (IsKeyDown(KEY_Q)) CameraRoll(camera, -CAMERA_ROTATION_SPEED);
	// if (IsKeyDown(KEY_E)) CameraRoll(camera, CAMERA_ROTATION_SPEED);

	CameraYaw(camera, -mousePositionDelta.x * CAMERA_MOUSE_MOVE_SENSITIVITY, rotateAroundTarget);
	CameraPitch(camera, -mousePositionDelta.y * CAMERA_MOUSE_MOVE_SENSITIVITY, lockView, rotateAroundTarget, rotateUp);

	// Camera movement
	if (IsKeyDown(KEY_W))
		CameraMoveForward(camera, 20 * CAMERA_MOVE_SPEED, moveInWorldPlane);
	if (IsKeyDown(KEY_A))
		CameraMoveRight(camera, -20 * CAMERA_MOVE_SPEED, moveInWorldPlane);
	if (IsKeyDown(KEY_S))
		CameraMoveForward(camera, -20 * CAMERA_MOVE_SPEED, moveInWorldPlane);
	if (IsKeyDown(KEY_D))
		CameraMoveRight(camera, 20 * CAMERA_MOVE_SPEED, moveInWorldPlane);
	if (IsKeyDown(KEY_SPACE))
		CameraMoveUp(camera, 20 * CAMERA_MOVE_SPEED);
	if (IsKeyDown(KEY_LEFT_CONTROL))
		CameraMoveUp(camera, -20 * CAMERA_MOVE_SPEED);
}